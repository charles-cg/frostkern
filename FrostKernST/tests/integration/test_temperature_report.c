#include "app_temperature_report.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    AppTemperatureQuality quality;
    AppSample sample={.valid=APP_SAMPLE_VALID_TEMP,.timestamp_ms=100U,
                      .temperature_centi_c=2500};
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,&quality)==2500);
    assert(quality==APP_TEMPERATURE_VALID);
    assert(AppTemperatureReport_Value(&sample,5100U,100,10000,&quality)==2500);
    assert(AppTemperatureReport_Value(&sample,5101U,100,10000,&quality)==INT16_MIN);
    assert(quality==APP_TEMPERATURE_STALE);
    sample.valid=0U;
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,&quality)==INT16_MIN);
    assert(quality==APP_TEMPERATURE_NO_DATA);
    sample.valid=APP_SAMPLE_VALID_TEMP;sample.temperature_centi_c=100;
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,&quality)==100);
    sample.temperature_centi_c=10000;
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,&quality)==10000);
    sample.temperature_centi_c=99;
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,&quality)==INT16_MIN);
    assert(quality==APP_TEMPERATURE_OUT_OF_RANGE);
    sample.temperature_centi_c=10001;
    assert(AppTemperatureReport_Value(&sample,100U,100,10000,NULL)==INT16_MIN);
    sample.temperature_centi_c=INT16_MIN;
    assert(AppTemperatureReport_Value(&sample,100U,INT16_MIN,10000,&quality)==INT16_MIN);
    assert(quality==APP_TEMPERATURE_OUT_OF_RANGE);
    assert(AppTemperatureReport_Value(NULL,100U,100,10000,&quality)==INT16_MIN);
    sample.temperature_centi_c=2500;sample.timestamp_ms=UINT32_MAX-999U;
    assert(AppTemperatureReport_Value(&sample,1000U,100,10000,&quality)==2500);
    assert(quality==APP_TEMPERATURE_VALID);
    assert(AppTemperatureReport_Value(&sample,1000U,10000,100,&quality)==INT16_MIN);
    puts("Temperature freshness: validity, bounds, unknown, stale and clock rollover passed.");
    return 0;
}
