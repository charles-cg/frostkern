#ifndef FROSTKERN_APP_UART_PROTOCOL_H
#define FROSTKERN_APP_UART_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_UART_PROTOCOL_VERSION 1U
#define APP_UART_SOF 0xA5U
#define APP_UART_MAX_PAYLOAD 64U
#define APP_UART_MAX_FRAME (APP_UART_MAX_PAYLOAD + 6U)

#define APP_UART_HEARTBEAT 0x01U
#define APP_UART_PING 0x02U
#define APP_UART_PONG 0x03U
#define APP_UART_TELEMETRY_TEMP 0x10U
#define APP_UART_DEVICE_JOINED 0x11U
/* Extension compatible: la trama TEMP 0x10 conserva sus 21 bytes. */
#define APP_UART_TELEMETRY_FLOW 0x12U
#define APP_UART_PERMIT_JOIN 0x20U
#define APP_UART_SET_SETPOINT 0x21U /* Reserved; this version returns NACK. */
#define APP_UART_MASTER_AIR 0x13U
#define APP_UART_FAN_STATUS 0x14U
#define APP_UART_SET_AC_FAN 0x22U
#define APP_UART_SET_AUTO_FAN 0x23U
#define APP_UART_FAN_BENCH 0x24U
#define APP_UART_ACK 0x7EU
#define APP_UART_NACK 0x7FU

#define APP_UART_OK_QUEUED 0U
#define APP_UART_ERR_LENGTH 1U
#define APP_UART_ERR_VALUE 2U
#define APP_UART_ERR_NOT_READY 3U
#define APP_UART_ERR_QUEUE_FULL 4U
#define APP_UART_ERR_UNSUPPORTED 5U

#define APP_UART_TEMP_VALID 0x01U
#define APP_UART_TEMP_STALE 0x02U
#define APP_UART_TEMP_REPORT_SEEN 0x04U

typedef struct {
    uint8_t type;
    uint8_t sequence;
    uint8_t length;
    uint8_t payload[APP_UART_MAX_PAYLOAD];
} AppUartFrame;

typedef struct {
    uint8_t bytes[APP_UART_MAX_FRAME];
    size_t used;
    uint32_t crc_errors;
} AppUartParser;

uint16_t AppUart_Crc16(const uint8_t *bytes, size_t length);
size_t AppUart_Encode(uint8_t type, uint8_t sequence,
                     const uint8_t *payload, size_t length,
                     uint8_t *output, size_t capacity);
void AppUartParser_Init(AppUartParser *parser);
void AppUartParser_Reset(AppUartParser *parser);
void AppUartParser_Push(AppUartParser *parser, uint8_t byte);
bool AppUartParser_Next(AppUartParser *parser, AppUartFrame *frame);

#endif
