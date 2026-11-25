#ifndef FROSTKERN_APP_FLOW_REPORT_H
#define FROSTKERN_APP_FLOW_REPORT_H
#include <stddef.h>
#include "app_types.h"
/* Private FrostKern cluster: exact mL/min, NOT the standard Flow Measurement
 * cluster's units. All boards must use this same extension. No new library. */
#define APP_ZB_FLOW_CLUSTER_ID 0xFC10U
#define APP_ZB_FLOW_ATTRIBUTE_ID 0x0000U
#define APP_ZB_FLOW_UNKNOWN UINT32_MAX
#define APP_FLOW_SAMPLE_MAX_AGE_MS 5000U
static inline uint32_t AppFlowReport_Value(const AppSample *sample, uint32_t now)
{
    if (sample == NULL || (sample->valid & APP_SAMPLE_VALID_FLOW) == 0U ||
        (uint32_t)(now - sample->timestamp_ms) > APP_FLOW_SAMPLE_MAX_AGE_MS) {
        return APP_ZB_FLOW_UNKNOWN;
    }
    return sample->flow_ml_min; /* Zero is valid when there are no pulses. */
}
#endif
