#include <Arduino.h>
#include <esp_system.h>
// Local credentials stay outside Git. A fresh clone builds with Wi-Fi off.
#if __has_include("wifi_config.h")
#include "wifi_config.h"
#else
#include "wifi_config.example.h"
#endif
#if FROSTKERN_WIFI_ENABLED
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#endif
extern "C" {
#include "app_uart_protocol.h"
}

#if !defined(ARDUINO_XIAO_ESP32C5)
#error "Selecciona la placa XIAO_ESP32C5 en Arduino IDE."
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Activa Tools/Herramientas > USB CDC On Boot > Enabled."
#endif

// XIAO ESP32-C5: D7 = GPIO12 (RX), D6 = GPIO11 (TX).
// No se envian comandos al STM32. Para la prueba externa, deja D6 sin conectar.
static AppUartParser parser;
static uint32_t rxBytes, goodFrames, badMessages;
static uint32_t lastByteMs, lastStatsMs;

static uint32_t read32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void printIeee(const uint8_t *p) {
  Serial.print("0x");
  for (unsigned i = 0; i < 8; ++i) Serial.printf("%02X", (unsigned)p[i]);
}

static void showFrame(const AppUartFrame *f) {
  const uint8_t *p = f->payload;
  uint8_t expected = 0;
  switch (f->type) {
    case APP_UART_HEARTBEAT: expected = 22; break;
    case APP_UART_PONG: expected = 6; break;
    case APP_UART_TELEMETRY_TEMP: expected = 21; break;
    case APP_UART_TELEMETRY_FLOW: expected = 23; break;
    case APP_UART_DEVICE_JOINED: expected = 10; break;
    default:
      Serial.printf("[UART] tipo=0x%02X len=%u seq=%u (sin decodificar)\r\n",
                    f->type, f->length, f->sequence);
      return;
  }
  if (f->length != expected || p[0] != APP_UART_PROTOCOL_VERSION) {
    ++badMessages;
    Serial.printf("[UART] mensaje incompatible: tipo=0x%02X len=%u\r\n",
                  f->type, f->length);
    return;
  }
  if (f->type == APP_UART_HEARTBEAT) {
    Serial.printf("[MASTER] red_iniciada=%u uptime_ms=%lu rx_dropped=%lu "
                  "uart_errors=%lu crc_errors=%lu tx_errors=%lu\r\n",
                  p[1], (unsigned long)read32(p + 2),
                  (unsigned long)read32(p + 6), (unsigned long)read32(p + 10),
                  (unsigned long)read32(p + 14), (unsigned long)read32(p + 18));
  } else if (f->type == APP_UART_PONG) {
    Serial.printf("[PONG] red_iniciada=%u uptime_ms=%lu\r\n",
                  p[1], (unsigned long)read32(p + 2));
  } else if (f->type == APP_UART_DEVICE_JOINED) {
    Serial.printf("[REGISTRO] Slave %u IEEE=", p[1]);
    printIeee(p + 2);
    Serial.println();
  } else if (f->type == APP_UART_TELEMETRY_FLOW) {
    const uint8_t flags = p[10];
    const bool stale = (flags & APP_UART_TEMP_STALE) != 0;
    const bool seen = (flags & APP_UART_TEMP_REPORT_SEEN) != 0;
    const bool valid = (flags & APP_UART_TEMP_VALID) != 0 && !stale && seen;
    Serial.printf("[FLOW] Slave %u IEEE=", p[1]);
    printIeee(p + 2);
    if (valid) {
      const uint32_t flow = read32(p + 11);
      Serial.printf(" Q=%lu mL/min (%lu.%03lu L/min)",
        (unsigned long)flow, (unsigned long)(flow / 1000U),
        (unsigned long)(flow % 1000U));
    } else { Serial.print(" Q=NO DISPONIBLE"); }
    Serial.printf(" valid=%u stale=%u report_seen=%u updated_at_ms=%lu age_ms=",
      valid, stale, seen, (unsigned long)read32(p + 15));
    const uint32_t age = read32(p + 19);
    if (age == UINT32_MAX) Serial.println("sin reporte");
    else Serial.println((unsigned long)age);
  } else {
    const uint8_t flags = p[10];
    const bool stale = (flags & APP_UART_TEMP_STALE) != 0;
    const bool seen = (flags & APP_UART_TEMP_REPORT_SEEN) != 0;
    const bool valid = (flags & APP_UART_TEMP_VALID) != 0 && !stale && seen;
    Serial.printf("[TEMP] Slave %u IEEE=", p[1]);
    printIeee(p + 2);
    if (valid) {
      const uint16_t raw = (uint16_t)p[11] | ((uint16_t)p[12] << 8);
      const int32_t centi = raw < 0x8000U ? (int32_t)raw : (int32_t)raw - 65536;
      const int32_t magnitude = centi < 0 ? -centi : centi;
      Serial.printf(" T=%s%ld.%02ld C", centi < 0 ? "-" : "",
                    (long)(magnitude / 100), (long)(magnitude % 100));
    } else {
      Serial.print(" T=NO DISPONIBLE");
    }
    Serial.printf(" valid=%u stale=%u report_seen=%u updated_at_ms=%lu age_ms=",
                  valid, stale, seen, (unsigned long)read32(p + 13));
    const uint32_t age = read32(p + 17);
    if (age == UINT32_MAX) Serial.println("sin reporte");
    else Serial.println((unsigned long)age);
  }
}

// Why the chip last restarted; diagnoses silent reboots.
static const char *resetReasonName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON (power lost or first power-up)";
    case ESP_RST_BROWNOUT: return "BROWNOUT (supply voltage dipped)";
    case ESP_RST_PWR_GLITCH: return "PWR_GLITCH (supply glitch)";
    case ESP_RST_PANIC: return "PANIC (firmware crash)";
    case ESP_RST_INT_WDT: return "INT_WDT (interrupt watchdog)";
    case ESP_RST_TASK_WDT: return "TASK_WDT (task watchdog)";
    case ESP_RST_WDT: return "WDT (other watchdog)";
    case ESP_RST_SW: return "SW (esp_restart)";
    case ESP_RST_EXT: return "EXT (reset pin)";
    case ESP_RST_USB: return "USB (USB reset, e.g. upload)";
    default: return "OTHER";
  }
}

// Optional station connection. UART keeps running if Wi-Fi is unavailable.
static void serviceWifi() {
#if FROSTKERN_WIFI_ENABLED
  static bool started = false, connected = false;
  static uint32_t lastAttempt = 0;
  const uint32_t now = millis();
  if (!started) {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(FROSTKERN_WIFI_SSID, FROSTKERN_WIFI_PASSWORD);
    started = true; lastAttempt = now;
    Serial.println("[WIFI] conectando; UART sigue activo");
  }
  const bool ready = WiFi.status() == WL_CONNECTED;
  if (ready != connected) {
    connected = ready;
    if (ready) {
      Serial.print("[WIFI] conectado, IP=");
      Serial.println(WiFi.localIP().toString().c_str());
    } else { Serial.println("[WIFI] desconectado; UART sigue activo"); }
  }
  if (!ready && (uint32_t)(now - lastAttempt) >= 30000U) {
    lastAttempt = now;
    WiFi.reconnect();
  }
#endif
}

#if FROSTKERN_WIFI_ENABLED
// E1: PATCH a fixed JSON to /test to prove Wi-Fi, TLS and the secret work.
static void uploadTest() {
  char url[256];
  const int n = snprintf(url, sizeof url, "%s/test.json?auth=%s",
                         FROSTKERN_FIREBASE_URL, FROSTKERN_FIREBASE_SECRET);
  if (n < 0 || (size_t)n >= sizeof url) {
    Serial.println("[FB] URL demasiado larga");
    return;
  }
  WiFiClientSecure client;
  client.setInsecure();  // E1 only: skips certificate check.
  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("[FB] begin fallo");
    return;
  }
  http.addHeader("Content-Type", "application/json");
  const int code = http.PATCH("{\"hello\":1,\"ts\":{\".sv\":\"timestamp\"}}");
  Serial.printf("[FB] PATCH -> %d\r\n", code);
  http.end();
}
#endif

void setup() {
  Serial.begin(115200);
  Serial1.setRxBufferSize(1024);
  Serial1.begin(115200, SERIAL_8N1, D7, D6);
  AppUartParser_Init(&parser);
  const uint32_t start = millis();
  while (!Serial && (uint32_t)(millis() - start) < 2500U) delay(10);
  Serial.println("\r\nFrostKern: receptor UART v1 + temperatura/caudal para XIAO ESP32-C5");
  Serial.println("RX=D7/GPIO12, 115200 8N1. No envia comandos.");
  const esp_reset_reason_t reason = esp_reset_reason();
  Serial.printf("[BOOT] reset_reason=%d %s\r\n", (int)reason, resetReasonName(reason));
  Serial.println("Esperando al Master. [RX] bytes=0 es normal sin conectar PA12.");
}

void loop() {
  serviceWifi();
  while (Serial1.available() > 0) {
    const uint32_t now = millis();
    if (parser.used != 0 && (uint32_t)(now - lastByteMs) >= 100U)
      AppUartParser_Reset(&parser);
    lastByteMs = now;
    const int byte = Serial1.read();
    if (byte < 0) break;
    ++rxBytes;
    AppUartParser_Push(&parser, (uint8_t)byte);
    AppUartFrame frame;
    while (AppUartParser_Next(&parser, &frame)) {
      ++goodFrames;
      showFrame(&frame);
    }
  }
  const uint32_t now = millis();
  if ((uint32_t)(now - lastStatsMs) >= 5000U) {
    lastStatsMs = now;
    Serial.printf("[RX] bytes=%lu tramas_crc_ok=%lu errores_crc=%lu incompatibles=%lu\r\n",
                  (unsigned long)rxBytes, (unsigned long)goodFrames,
                  (unsigned long)parser.crc_errors, (unsigned long)badMessages);
  }
#if FROSTKERN_WIFI_ENABLED
  static uint32_t lastUploadMs;
  if (WiFi.status() == WL_CONNECTED && (uint32_t)(now - lastUploadMs) >= 5000U) {
    lastUploadMs = now;
    uploadTest();
  }
#endif
  delay(1);
}
