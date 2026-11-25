/* FrostKern ESP32 bridge: STM32 Master UART frames -> Firebase Realtime Database.
 *
 * Tasks:
 *   uart_task    reads USART frames (docs: FrostKernST/docs/UART_PROTOCOL.md),
 *                keeps the latest temperature/flow per slave.
 *   upload_task  every 2 s PATCHes changed slaves to /slaves/{IEEE}/telemetry,
 *                every 30 s PATCHes /bridge. One keep-alive HTTPS connection.
 * Wi-Fi reconnects forever; UART keeps running without Wi-Fi.
 */
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "driver/uart.h"
#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#include "app_uart_protocol.h"

/* XIAO ESP32-C5: D7 = GPIO12 <- STM32 PA12 (TX), D6 = GPIO11 -> STM32 PA11.
 * GPIO11/12 are also UART0's pins, so the console must be on USB (sdkconfig). */
#define BRIDGE_UART UART_NUM_1
#define BRIDGE_RX_GPIO 12
#define BRIDGE_TX_GPIO 11
#define BRIDGE_BAUD 115200
#define FRAME_GAP_US 100000 /* Protocol: a pause >= 100 ms drops a partial frame. */

#define MAX_SLAVES 2
#define UPLOAD_PERIOD_MS 2000
#define BRIDGE_PERIOD_MS 30000
#define STATS_PERIOD_US 5000000

#define WIFI_CONNECTED_BIT BIT0

static const char *TAG = "frostkern";

typedef struct {
    bool known;
    char ieee[17]; /* 16 upper-case hex digits, no 0x: the Firebase key. */
    uint8_t node_id;
    bool temp_valid;
    int16_t temp_centi;
    uint32_t temp_updated_at;
    bool flow_valid;
    uint32_t flow_ml_min;
    uint32_t flow_updated_at;
    bool telemetry_dirty;
    bool info_dirty;
} SlaveState;

typedef struct {
    bool seen;
    bool network_started;
    uint32_t uptime_ms;
} MasterState;

static SlaveState s_slaves[MAX_SLAVES];
static MasterState s_master;
static SemaphoreHandle_t s_lock;
static EventGroupHandle_t s_wifi_events;
static esp_http_client_handle_t s_http;

static void format_temp(char *out, size_t cap, bool valid, int16_t centi);

/* ---------- Wire decoding ---------- */

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void format_ieee(const uint8_t *p, char out[17])
{
    for (unsigned i = 0; i < 8; ++i) {
        snprintf(out + 2 * i, 3, "%02X", (unsigned)p[i]);
    }
}

/* A reading counts only when VALID is set, STALE is clear and a report was seen. */
static bool reading_valid(uint8_t flags)
{
    return (flags & APP_UART_TEMP_VALID) && !(flags & APP_UART_TEMP_STALE) &&
           (flags & APP_UART_TEMP_REPORT_SEEN);
}

/* Returns the slave for node_id 1..MAX_SLAVES, updating its IEEE. Lock must be held. */
static SlaveState *slave_for(uint8_t node_id, const uint8_t *ieee_bytes)
{
    if (node_id < 1 || node_id > MAX_SLAVES) {
        return NULL;
    }
    SlaveState *s = &s_slaves[node_id - 1];
    char ieee[17];
    format_ieee(ieee_bytes, ieee);
    if (!s->known || strcmp(s->ieee, ieee) != 0) {
        memset(s, 0, sizeof *s);
        s->known = true;
        s->node_id = node_id;
        memcpy(s->ieee, ieee, sizeof ieee);
        s->info_dirty = true;
    }
    return s;
}

static uint8_t expected_length(uint8_t type)
{
    switch (type) {
    case APP_UART_HEARTBEAT: return 22;
    case APP_UART_PONG: return 6;
    case APP_UART_TELEMETRY_TEMP: return 21;
    case APP_UART_TELEMETRY_FLOW: return 23;
    case APP_UART_DEVICE_JOINED: return 10;
    default: return 0;
    }
}

/* Returns false for a frame of a known type with the wrong length or version. */
static bool handle_frame(const AppUartFrame *f)
{
    const uint8_t expected = expected_length(f->type);
    if (expected == 0) {
        return true; /* Unknown/unused type (ACK, NACK...): ignore. */
    }
    const uint8_t *p = f->payload;
    if (f->length != expected || p[0] != APP_UART_PROTOCOL_VERSION) {
        return false;
    }

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (f->type == APP_UART_HEARTBEAT) {
        s_master.seen = true;
        s_master.network_started = p[1] != 0;
        s_master.uptime_ms = read_u32(p + 2);
    } else if (f->type == APP_UART_DEVICE_JOINED) {
        SlaveState *s = slave_for(p[1], p + 2);
        if (s != NULL) {
            s->info_dirty = true;
            ESP_LOGI(TAG, "[REGISTRO] slave %u IEEE=%s", s->node_id, s->ieee);
        }
    } else if (f->type == APP_UART_TELEMETRY_TEMP) {
        SlaveState *s = slave_for(p[1], p + 2);
        if (s != NULL) {
            const bool valid = reading_valid(p[10]);
            const int16_t centi = (int16_t)((uint16_t)p[11] | ((uint16_t)p[12] << 8));
            const uint32_t updated_at = read_u32(p + 13);
            /* Frames repeat every second; upload only a new report or a validity change. */
            if (valid != s->temp_valid || updated_at != s->temp_updated_at ||
                (valid && centi != s->temp_centi)) {
                s->temp_valid = valid;
                s->temp_centi = valid ? centi : 0;
                s->temp_updated_at = updated_at;
                s->telemetry_dirty = true;
                char text[16];
                format_temp(text, sizeof text, valid, centi);
                ESP_LOGI(TAG, "[TEMP] slave %u tempC=%s", s->node_id, text);
            }
        }
    } else if (f->type == APP_UART_TELEMETRY_FLOW) {
        SlaveState *s = slave_for(p[1], p + 2);
        if (s != NULL) {
            const bool valid = reading_valid(p[10]);
            const uint32_t flow = read_u32(p + 11);
            const uint32_t updated_at = read_u32(p + 15);
            if (valid != s->flow_valid || updated_at != s->flow_updated_at ||
                (valid && flow != s->flow_ml_min)) {
                s->flow_valid = valid;
                s->flow_ml_min = valid ? flow : 0;
                s->flow_updated_at = updated_at;
                s->telemetry_dirty = true;
                if (valid) {
                    ESP_LOGI(TAG, "[FLOW] slave %u %" PRIu32 " mL/min", s->node_id, flow);
                } else {
                    ESP_LOGI(TAG, "[FLOW] slave %u no valid reading", s->node_id);
                }
            }
        }
    }
    xSemaphoreGive(s_lock);
    return true;
}

static void uart_task(void *arg)
{
    (void)arg;
    const uart_config_t cfg = {
        .baud_rate = BRIDGE_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(BRIDGE_UART, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(BRIDGE_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BRIDGE_UART, BRIDGE_TX_GPIO, BRIDGE_RX_GPIO, UART_PIN_NO_CHANGE,
                                 UART_PIN_NO_CHANGE));

    static AppUartParser parser;
    AppUartParser_Init(&parser);
    uint8_t buf[128];
    uint32_t rx_bytes = 0, good_frames = 0, bad_messages = 0;
    int64_t last_byte_us = 0, last_stats_us = esp_timer_get_time();

    for (;;) {
        const int n = uart_read_bytes(BRIDGE_UART, buf, sizeof buf, pdMS_TO_TICKS(20));
        const int64_t now = esp_timer_get_time();
        if (n > 0) {
            if (parser.used != 0 && now - last_byte_us >= FRAME_GAP_US) {
                AppUartParser_Reset(&parser);
            }
            last_byte_us = now;
            rx_bytes += (uint32_t)n;
            for (int i = 0; i < n; ++i) {
                AppUartParser_Push(&parser, buf[i]);
                AppUartFrame frame;
                while (AppUartParser_Next(&parser, &frame)) {
                    ++good_frames;
                    if (!handle_frame(&frame)) {
                        ++bad_messages;
                    }
                }
            }
        }
        if (now - last_stats_us >= STATS_PERIOD_US) {
            last_stats_us = now;
            ESP_LOGI(TAG, "[RX] bytes=%" PRIu32 " frames_ok=%" PRIu32 " crc_errors=%" PRIu32
                          " incompatible=%" PRIu32,
                     rx_bytes, good_frames, parser.crc_errors, bad_messages);
        }
    }
}

/* ---------- Firebase ---------- */

/* PATCH json to {URL}{path}.json. Returns the HTTP status, or -1 on a transport error.
 * The handle stays open between calls (keep-alive), so TLS is negotiated once. */
static int firebase_patch(const char *path, const char *json)
{
    char url[512];
    const int n = snprintf(url, sizeof url, "%s%s.json?auth=%s", CONFIG_FROSTKERN_FIREBASE_URL, path,
                           CONFIG_FROSTKERN_FIREBASE_SECRET);
    if (n < 0 || (size_t)n >= sizeof url) {
        ESP_LOGE(TAG, "[FB] URL too long");
        return -1;
    }
    if (s_http == NULL) {
        const esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_PATCH,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .keep_alive_enable = true,
            .timeout_ms = 8000,
        };
        s_http = esp_http_client_init(&config);
        if (s_http == NULL) {
            return -1;
        }
        esp_http_client_set_header(s_http, "Content-Type", "application/json");
    } else {
        esp_http_client_set_url(s_http, url);
    }
    esp_http_client_set_method(s_http, HTTP_METHOD_PATCH);
    esp_http_client_set_post_field(s_http, json, (int)strlen(json));
    const esp_err_t err = esp_http_client_perform(s_http);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "[FB] PATCH %s failed: %s", path, esp_err_to_name(err));
        esp_http_client_cleanup(s_http); /* Start a fresh connection next time. */
        s_http = NULL;
        return -1;
    }
    return esp_http_client_get_status_code(s_http);
}

static void format_temp(char *out, size_t cap, bool valid, int16_t centi)
{
    if (!valid) {
        snprintf(out, cap, "null"); /* null deletes the key: the app shows "no data". */
        return;
    }
    const int32_t v = centi;
    const uint32_t mag = (uint32_t)(v < 0 ? -v : v);
    snprintf(out, cap, "%s%" PRIu32 ".%02" PRIu32, v < 0 ? "-" : "", mag / 100U, mag % 100U);
}

static void format_flow(char *out, size_t cap, bool valid, uint32_t ml_min)
{
    if (!valid) {
        snprintf(out, cap, "null");
        return;
    }
    snprintf(out, cap, "%" PRIu32 ".%03" PRIu32, ml_min / 1000U, ml_min % 1000U);
}

static void upload_slaves(void)
{
    for (unsigned i = 0; i < MAX_SLAVES; ++i) {
        xSemaphoreTake(s_lock, portMAX_DELAY);
        SlaveState snap = s_slaves[i];
        s_slaves[i].telemetry_dirty = false;
        s_slaves[i].info_dirty = false;
        xSemaphoreGive(s_lock);
        if (!snap.known) {
            continue;
        }

        char path[64];
        char json[192];
        if (snap.info_dirty) {
            snprintf(path, sizeof path, "/slaves/%s/info", snap.ieee);
            snprintf(json, sizeof json, "{\"nodeId\":%u,\"registered\":true,\"ts\":{\".sv\":\"timestamp\"}}",
                     snap.node_id);
            const int code = firebase_patch(path, json);
            ESP_LOGI(TAG, "[FB] info slave %u -> %d", snap.node_id, code);
            if (code != 200) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_slaves[i].info_dirty = true; /* Retry next round. */
                xSemaphoreGive(s_lock);
            }
        }
        if (snap.telemetry_dirty) {
            char temp[16], flow[16];
            format_temp(temp, sizeof temp, snap.temp_valid, snap.temp_centi);
            format_flow(flow, sizeof flow, snap.flow_valid, snap.flow_ml_min);
            snprintf(path, sizeof path, "/slaves/%s/telemetry", snap.ieee);
            snprintf(json, sizeof json,
                     "{\"tempC\":%s,\"flowLpm\":%s,\"nodeId\":%u,\"ts\":{\".sv\":\"timestamp\"}}", temp,
                     flow, snap.node_id);
            const int code = firebase_patch(path, json);
            ESP_LOGI(TAG, "[FB] telemetry slave %u %s -> %d", snap.node_id, json, code);
            if (code != 200) {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                s_slaves[i].telemetry_dirty = true;
                xSemaphoreGive(s_lock);
            }
        }
    }
}

static void upload_bridge(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const MasterState m = s_master;
    xSemaphoreGive(s_lock);
    char json[192];
    snprintf(json, sizeof json,
             "{\"lastSeen\":{\".sv\":\"timestamp\"},\"masterSeen\":%s,\"networkStarted\":%s,"
             "\"masterUptimeMs\":%" PRIu32 "}",
             m.seen ? "true" : "false", m.network_started ? "true" : "false", m.uptime_ms);
    ESP_LOGI(TAG, "[FB] bridge -> %d", firebase_patch("/bridge", json));
}

static void upload_task(void *arg)
{
    (void)arg;
    TickType_t last_bridge = 0;
    bool bridge_sent = false;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(UPLOAD_PERIOD_MS));
        if (!(xEventGroupGetBits(s_wifi_events) & WIFI_CONNECTED_BIT)) {
            continue;
        }
        upload_slaves();
        const TickType_t now = xTaskGetTickCount();
        if (!bridge_sent || now - last_bridge >= pdMS_TO_TICKS(BRIDGE_PERIOD_MS)) {
            last_bridge = now;
            bridge_sent = true;
            upload_bridge();
        }
    }
}

/* ---------- Wi-Fi (station, reconnects forever) ---------- */

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *e = data;
        xEventGroupClearBits(s_wifi_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "[WIFI] disconnected (reason %u), retrying", e->reason);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "[WIFI] connected, IP=" IPSTR, IP2STR(&e->ip_info.ip));
        xEventGroupSetBits(s_wifi_events, WIFI_CONNECTED_BIT);
    }
}

static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler,
                                                        NULL, NULL));
    wifi_config_t config = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_SSID,
            .password = CONFIG_ESP_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "[WIFI] connecting to \"%s\"; UART keeps running", CONFIG_ESP_WIFI_SSID);
}

static const char *reset_reason_name(esp_reset_reason_t r)
{
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

void app_main(void)
{
    const esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "FrostKern bridge (ESP-IDF): UART RX=GPIO%d -> Firebase", BRIDGE_RX_GPIO);
    ESP_LOGI(TAG, "[BOOT] reset_reason=%d %s", (int)reason, reset_reason_name(reason));

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_lock = xSemaphoreCreateMutex();
    s_wifi_events = xEventGroupCreate();
    configASSERT(s_lock != NULL && s_wifi_events != NULL);

    xTaskCreate(uart_task, "uart", 4096, NULL, 6, NULL);
    wifi_start();
    xTaskCreate(upload_task, "upload", 8192, NULL, 5, NULL);
}
