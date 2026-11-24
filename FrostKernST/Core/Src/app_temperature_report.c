#include "app_temperature_report.h"
#include <stddef.h>

int16_t AppTemperatureReport_Value(const AppSample *sample, uint32_t now_ms,
    int16_t minimum_centi_c, int16_t maximum_centi_c,
    AppTemperatureQuality *quality)
{
    AppTemperatureQuality result = APP_TEMPERATURE_NO_DATA;
    int16_t value = APP_TEMPERATURE_REPORT_UNKNOWN;

    if (sample != NULL && (sample->valid & APP_SAMPLE_VALID_TEMP) != 0U) {
        if ((uint32_t)(now_ms - sample->timestamp_ms) >
            APP_TEMPERATURE_SAMPLE_MAX_AGE_MS) {
            result = APP_TEMPERATURE_STALE;
        } else if (minimum_centi_c > maximum_centi_c ||
                   sample->temperature_centi_c == APP_TEMPERATURE_REPORT_UNKNOWN ||
                   sample->temperature_centi_c < minimum_centi_c ||
                   sample->temperature_centi_c > maximum_centi_c) {
            result = APP_TEMPERATURE_OUT_OF_RANGE;
        } else {
            result = APP_TEMPERATURE_VALID;
            value = sample->temperature_centi_c;
        }
    }
    if (quality != NULL) {
        *quality = result;
    }
    return value;
}
