#include "app_uart_protocol.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void Feed(AppUartParser *parser, const uint8_t *bytes, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        AppUartParser_Push(parser, bytes[i]);
    }
}

int main(void)
{
    assert(AppUart_Crc16((const uint8_t *)"123456789", 9) == 0x29B1U);
    uint8_t encoded[APP_UART_MAX_FRAME];
    uint8_t payload[APP_UART_MAX_PAYLOAD];
    for (size_t i = 0; i < sizeof(payload); ++i) {
        payload[i] = (uint8_t)i;
    }
    payload[5] = APP_UART_SOF;
    AppUartParser parser;
    AppUartFrame frame;

    /* Independently checked with Python binascii.crc_hqx(body, 0xffff). */
    const uint8_t pingVector[] = {0xA5U, 0x00U, 0x02U, 0x01U, 0xDFU, 0xBAU};
    assert(AppUart_Encode(APP_UART_PING, 1U, NULL, 0U,
                         encoded, sizeof(encoded)) == sizeof(pingVector));
    assert(memcmp(encoded, pingVector, sizeof(pingVector)) == 0);
    const uint8_t joinVector[] = {0xA5U, 0x01U, 0x20U, 0x02U, 0xB4U, 0x8FU, 0xF5U};
    const uint8_t seconds = 180U;
    assert(AppUart_Encode(APP_UART_PERMIT_JOIN, 2U, &seconds, 1U,
                         encoded, sizeof(encoded)) == sizeof(joinVector));
    assert(memcmp(encoded, joinVector, sizeof(joinVector)) == 0);

    for (size_t length = 0; length <= APP_UART_MAX_PAYLOAD; ++length) {
        AppUartParser_Init(&parser);
        size_t size = AppUart_Encode(0x20U, 0xA5U, payload, length,
                                    encoded, sizeof(encoded));
        assert(size == length + 6U);
        for (size_t i = 0; i < size; ++i) {
            AppUartParser_Push(&parser, encoded[i]);
            bool complete = AppUartParser_Next(&parser, &frame);
            assert(complete == (i == size - 1U));
        }
        assert(frame.type == 0x20U && frame.sequence == 0xA5U);
        assert(frame.length == length);
        assert(memcmp(frame.payload, payload, length) == 0);
        assert(!AppUartParser_Next(&parser, &frame));
    }
    assert(AppUart_Encode(1, 2, payload, 65, encoded, sizeof(encoded)) == 0);
    assert(AppUart_Encode(1, 2, NULL, 1, encoded, sizeof(encoded)) == 0);
    assert(AppUart_Encode(1, 2, NULL, 0, encoded, 5) == 0);
    assert(AppUart_Encode(1, 2, NULL, 0, NULL, 6) == 0);

    /* CRC corruption is rejected, and the following valid frame is recovered. */
    size_t size = AppUart_Encode(APP_UART_PING, 7, NULL, 0, encoded, sizeof(encoded));
    AppUartParser_Init(&parser);
    encoded[4] ^= 1U;
    Feed(&parser, encoded, size);
    assert(!AppUartParser_Next(&parser, &frame));
    assert(parser.crc_errors != 0U);
    encoded[4] ^= 1U;
    Feed(&parser, encoded, size);
    assert(AppUartParser_Next(&parser, &frame));
    assert(frame.sequence == 7U);

    /* Boot noise, invalid LEN, adjacent frames, and a truncated frame reset. */
    AppUartParser_Init(&parser);
    const uint8_t noise[] = {'b', 'o', 'o', 't', 0xA5U, 0xFFU};
    Feed(&parser, noise, sizeof(noise));
    Feed(&parser, encoded, size);
    Feed(&parser, encoded, size);
    assert(AppUartParser_Next(&parser, &frame));
    assert(AppUartParser_Next(&parser, &frame));
    assert(!AppUartParser_Next(&parser, &frame));
    Feed(&parser, encoded, 3);
    assert(!AppUartParser_Next(&parser, &frame));
    AppUartParser_Reset(&parser);
    Feed(&parser, encoded, size);
    assert(AppUartParser_Next(&parser, &frame));

    /* A corrupted long LEN can conceal complete frames: drain recovers both. */
    AppUartParser_Init(&parser);
    const uint8_t badHeader[] = {0xA5U, 20U, 0x44U, 0x44U};
    Feed(&parser, badHeader, sizeof(badHeader));
    Feed(&parser, encoded, size);
    Feed(&parser, encoded, size);
    uint8_t zeros[10] = {0};
    Feed(&parser, zeros, sizeof(zeros));
    assert(AppUartParser_Next(&parser, &frame));
    assert(AppUartParser_Next(&parser, &frame));
    assert(!AppUartParser_Next(&parser, &frame));

    /* Deterministic noise stress under ASan/UBSan. */
    uint32_t random = 12345U;
    for (unsigned int i = 0; i < 200000U; ++i) {
        random = random * 1664525U + 1013904223U;
        AppUartParser_Push(&parser, (uint8_t)(random >> 24));
        while (AppUartParser_Next(&parser, &frame)) {
            assert(frame.length <= APP_UART_MAX_PAYLOAD);
        }
        assert(parser.used <= APP_UART_MAX_FRAME);
    }
    puts("UART protocol: CRC vector, 65 payload lengths, corruption, resync, boundaries and noise passed.");
    return 0;
}
