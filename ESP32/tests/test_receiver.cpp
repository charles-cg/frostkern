#include "../FrostKern_UART_Receiver/FrostKern_UART_Receiver.ino"
#include <assert.h>
#include <string.h>

static void feed(uint8_t type, const uint8_t *p, size_t n, bool corrupt = false) {
  uint8_t b[APP_UART_MAX_FRAME];
  size_t total = AppUart_Encode(type, 1, p, n, b, sizeof b);
  assert(total == n + 6);
  if (corrupt) b[total - 1] ^= 1;
  for (size_t i = 0; i < total; ++i) Serial1.incoming.push_back(b[i]);
  loop();
}
static void has(const char *s) { assert(Serial.out.find(s) != std::string::npos); }
int main() {
  assert(AppUart_Crc16((const uint8_t *)"123456789", 9) == 0x29B1);
  uint8_t ping[6];
  assert(AppUart_Encode(APP_UART_PING, 1, nullptr, 0, ping, sizeof ping) == 6);
  const uint8_t reference[] = {0xA5,0,2,1,0xDF,0xBA};
  assert(memcmp(ping, reference, 6) == 0);
  setup();
  fakeMs = 5000; loop(); has("[RX] bytes=0");
  uint8_t hb[22] = {1,1}; hb[2] = 42;
  feed(APP_UART_HEARTBEAT, hb, sizeof hb);
  has("[MASTER] red_iniciada=1 uptime_ms=42");
  uint8_t t[21] = {1,1,0,0x80,0xE1,0xFF,0xFE,0x32,0xFC,0x18,5,0x44,9};
  feed(APP_UART_TELEMETRY_TEMP, t, sizeof t);
  has("IEEE=0x0080E1FFFE32FC18 T=23.72 C valid=1");
  t[11] = 0xCE; t[12] = 0xFF;
  feed(APP_UART_TELEMETRY_TEMP, t, sizeof t); has("T=-0.50 C");
  t[1] = 2; t[9] = 0x19; t[10] = 6; t[11] = t[12] = 0;
  feed(APP_UART_TELEMETRY_TEMP, t, sizeof t);
  has("Slave 2 IEEE=0x0080E1FFFE32FC19 T=NO DISPONIBLE valid=0 stale=1");
  uint8_t f[23] = {1,1,0,0x80,0xE1,0xFF,0xFE,0x32,0xFC,0x18,5};
  f[11] = 0x4C; f[12] = 0x1D; // 7500 mL/min
  feed(APP_UART_TELEMETRY_FLOW, f, sizeof f);
  has("Q=7500 mL/min (7.500 L/min) valid=1");
  f[11] = f[12] = 0;
  feed(APP_UART_TELEMETRY_FLOW, f, sizeof f);
  has("Q=0 mL/min (0.000 L/min) valid=1");
  f[10] = 6;
  feed(APP_UART_TELEMETRY_FLOW, f, sizeof f);
  has("Q=NO DISPONIBLE valid=0 stale=1 report_seen=1");
  const uint32_t before = goodFrames;
  feed(APP_UART_HEARTBEAT, hb, sizeof hb, true);
  assert(goodFrames == before && parser.crc_errors > 0);
  fakeMs += 101;
  feed(APP_UART_HEARTBEAT, hb, sizeof hb);
  assert(goodFrames == before + 1);
  feed(APP_UART_TELEMETRY_TEMP, t, 5); assert(badMessages == 1);
  // Noise and a truncated frame followed by an inter-byte timeout.
  Serial1.incoming.push_back(0x66); Serial1.incoming.push_back(0xA5);
  Serial1.incoming.push_back(21); loop(); fakeMs += 101;
  feed(APP_UART_HEARTBEAT, hb, sizeof hb);
  assert(goodFrames == before + 3);
  // Back-to-back heartbeat frames, as a single buffered serial burst.
  uint8_t encoded[28]; assert(AppUart_Encode(1, 7, hb, 22, encoded, 28) == 28);
  for (unsigned j = 0; j < 2; ++j)
    for (uint8_t b : encoded) Serial1.incoming.push_back(b);
  loop(); assert(goodFrames == before + 5);
#if FROSTKERN_WIFI_ENABLED
  assert(WiFi.begins==1U);
  WiFi.connected=true;loop();has("[WIFI] conectado, IP=192.0.2.10");
  WiFi.connected=false;fakeMs+=30001U;loop();
  assert(WiFi.reconnects>0U);
  feed(APP_UART_TELEMETRY_TEMP,t,sizeof t);
  has("T=NO DISPONIBLE"); // UART still processes frames with no AP.
#endif
  puts("PASS: CRC, wire vector, heartbeat, temperature, negative, stale, two IEEE, malformed, corruption, timeout and serial burst");
}
