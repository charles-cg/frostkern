#ifndef FROSTKERN_APP_TEMPERATURE_REPORT_H
#define FROSTKERN_APP_TEMPERATURE_REPORT_H

#include <stdint.h>
#include "app_types.h"

/* Bring-up SensorTask samples about once a second. Adjust this if that changes. */
#define APP_TEMPERATURE_SAMPLE_MAX_AGE_MS 5000U
#define APP_TEMPERATURE_REPORT_UNKNOWN INT16_MIN

typedef enum {
    APP_TEMPERATURE_NO_DATA = 0,
    APP_TEMPERATURE_VALID,
    APP_TEMPERATURE_STALE,
    APP_TEMPERATURE_OUT_OF_RANGE
} AppTemperatureQuality;

/* now_ms must use the same clock as sample.timestamp_ms (HAL_GetTick here).
 * This validates freshness and the already-advertised Zigbee range. It does
 * not calibrate the sensor or declare its physical measurement accuracy.
 */
int16_t AppTemperatureReport_Value(const AppSample *sample, uint32_t now_ms,
    int16_t minimum_centi_c, int16_t maximum_centi_c,
    AppTemperatureQuality *quality);

#endif
