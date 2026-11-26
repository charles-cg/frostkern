#include "app_uart_protocol.h"
#include <string.h>

uint16_t AppUart_Crc16(const uint8_t *bytes, size_t length)
{
    uint16_t crc = 0xFFFFU;
    for (size_t i = 0U; i < length; ++i) {
        crc ^= (uint16_t)((uint16_t)bytes[i] << 8U);
        for (unsigned int bit = 0U; bit < 8U; ++bit) {
            crc = (uint16_t)((crc & 0x8000U) != 0U
                ? ((uint32_t)crc << 1U) ^ 0x1021U
                : (uint32_t)crc << 1U);
        }
    }
    return crc;
}

size_t AppUart_Encode(uint8_t type, uint8_t sequence,
                     const uint8_t *payload, size_t length,
                     uint8_t *output, size_t capacity)
{
    if (output == NULL || length > APP_UART_MAX_PAYLOAD ||
        capacity < length + 6U || (length != 0U && payload == NULL)) {
        return 0U;
    }
    output[0] = APP_UART_SOF;
    output[1] = (uint8_t)length;
    output[2] = type;
    output[3] = sequence;
    if (length != 0U) {
        memcpy(&output[4], payload, length);
    }
    uint16_t crc = AppUart_Crc16(&output[1], length + 3U);
    output[length + 4U] = (uint8_t)crc;
    output[length + 5U] = (uint8_t)(crc >> 8U);
    return length + 6U;
}

static void DropPrefix(AppUartParser *parser, size_t length)
{
    parser->used -= length;
    memmove(parser->bytes, &parser->bytes[length], parser->used);
}

void AppUartParser_Init(AppUartParser *parser)
{
    memset(parser, 0, sizeof(*parser));
}

void AppUartParser_Reset(AppUartParser *parser)
{
    parser->used = 0U;
}

void AppUartParser_Push(AppUartParser *parser, uint8_t byte)
{
    if (parser->used == sizeof(parser->bytes)) {
        DropPrefix(parser, 1U);
    }
    parser->bytes[parser->used++] = byte;
}

bool AppUartParser_Next(AppUartParser *parser, AppUartFrame *frame)
{
    while (parser->used != 0U) {
        if (parser->bytes[0] != APP_UART_SOF) {
            DropPrefix(parser, 1U);
            continue;
        }
        if (parser->used < 2U) {
            return false;
        }
        size_t length = parser->bytes[1];
        if (length > APP_UART_MAX_PAYLOAD) {
            DropPrefix(parser, 1U);
            continue;
        }
        size_t total = length + 6U;
        if (parser->used < total) {
            return false;
        }
        uint16_t received = (uint16_t)parser->bytes[length + 4U] |
            (uint16_t)((uint16_t)parser->bytes[length + 5U] << 8U);
        if (received != AppUart_Crc16(&parser->bytes[1], length + 3U)) {
            ++parser->crc_errors;
            DropPrefix(parser, 1U);
            continue;
        }
        frame->length = (uint8_t)length;
        frame->type = parser->bytes[2];
        frame->sequence = parser->bytes[3];
        if (length != 0U) {
            memcpy(frame->payload, &parser->bytes[4], length);
        }
        DropPrefix(parser, total);
        return true;
    }
    return false;
}
