/*
 * ds18b20.h
 *
 *  Created on: 26 nov 2026
 *      Author: joaxr
 */

#ifndef INC_DS18B20_H_
#define INC_DS18B20_H_

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    DS18B20_OK,
    DS18B20_NO_PRESENCE,
    DS18B20_NOT_READY,
    DS18B20_RANGE_ERROR,
    DS18B20_CRC_ERROR
} DS18B20_Status;

DS18B20_Status DS18B20_DecodeScratchpad(const uint8_t data[9],int16_t *centiC);
/* Fault: 0=none, 1=idle low, 2=no presence at 70 us, 3=still low after reset. */
typedef struct {
    uint32_t counter, pclk1_hz;
    uint16_t prescaler;
    uint8_t fault, idle_high, presence_low, final_high, mode, open_drain;
    uint8_t odr_before, odr_after, input_high;
} DS18B20_Diagnostics;
void DS18B20_GetDiagnostics(DS18B20_Diagnostics *out);
void DS18B20_Init(void);
bool DS18B20_StartConversion(void);
DS18B20_Status DS18B20_ReadTemperature(int16_t *centiC);


#endif /* INC_DS18B20_H_ */
