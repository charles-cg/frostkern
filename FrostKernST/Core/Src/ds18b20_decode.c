#include "ds18b20.h"
#include <stddef.h>
static uint8_t Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0U;

    for (uint8_t i = 0U; i < length; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0U; bit < 8U; bit++) {
            uint8_t lsb = crc & 1U;
            crc >>= 1U;
            if (lsb != 0U) {
                crc ^= 0x8CU;
            }
        }
    }

    return crc;
}

DS18B20_Status DS18B20_DecodeScratchpad(const uint8_t data[9],int16_t *centiC)
{
    if(data==NULL || centiC==NULL) { return DS18B20_CRC_ERROR; }
    bool allZero=true;
    for(unsigned int i=0U;i<9U;++i) { if(data[i]!=0U) { allZero=false; } }
    if (allZero || Crc8(data, 8U) != data[8]) {
        return DS18B20_CRC_ERROR;
    }

    uint16_t bits = ((uint16_t)data[1] << 8U) | data[0];
    uint8_t resolution = (data[4] >> 5U) & 3U;
    uint16_t mask = (uint16_t)(0xFFFFU << (3U - resolution));
    int32_t scaled = (int32_t)(int16_t)(bits & mask) * 100;

    if(scaled < -5500*16 || scaled > 12500*16) { return DS18B20_RANGE_ERROR; }

    // El registro de temperatura tiene unidades de 1/16 C con signo.
    if (scaled < 0) {
        scaled -= 8;
    } else {
        scaled += 8;
    }

    *centiC = (int16_t)(scaled / 16);
    return DS18B20_OK;
}
