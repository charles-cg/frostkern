/* Event-driven post-join sampling. Runs ONLY in ZbAppTask, never callbacks. */
#include "app_common.h"
#include "main.h"
#include "app_zigbee.h"
#include "app_ipc.h"
#include "app_flow_report.h"
#include "app_network_config.h"
#include "log_module.h"
#include "zigbee.aps.h"
#include "zcl/zcl.h"
#include "zcl/general/zcl.temp.meas.h"

#if APP_IS_SLAVE
#define FRESH_TEMPERATURE 1U
#define FRESH_FLOW 2U
static uint8_t freshPending;
static uint32_t freshGeneration;
static uint32_t freshRequestMs;

static void RequestSample(void)
{
    freshGeneration = AppIpc_RequestFreshSample();
    freshRequestMs = HAL_GetTick();
    if (freshGeneration != 0U) {
        LOG_INFO_APP("[ZB-FRESH SLAVE] new measurement requested, generation=%lu",
            (unsigned long)freshGeneration);
    }
}

static void InvalidateAttributes(uint8_t mask)
{
    if ((mask & FRESH_TEMPERATURE) != 0U && stZigbeeAppInfo.temperatureCluster != NULL &&
        ZbZclAttrIntegerWrite(stZigbeeAppInfo.temperatureCluster,
            ZCL_TEMP_MEAS_ATTR_MEAS_VAL, (int16_t)ZCL_TEMP_MEAS_UNKNOWN) != ZCL_STATUS_SUCCESS) {
        LOG_ERROR_APP("[ZB-FRESH SLAVE] could not clear temperature attribute");
    }
    if ((mask & FRESH_FLOW) != 0U && stZigbeeAppInfo.flowCluster != NULL &&
        ZbZclAttrIntegerWrite(stZigbeeAppInfo.flowCluster,
            APP_ZB_FLOW_ATTRIBUTE_ID, APP_ZB_FLOW_UNKNOWN) != ZCL_STATUS_SUCCESS) {
        LOG_ERROR_APP("[ZB-FRESH SLAVE] could not clear flow attribute");
    }
}
#endif

void APP_ZIGBEE_RequestFreshReports(void)
{
#if APP_IS_SLAVE
    freshPending = FRESH_TEMPERATURE | FRESH_FLOW;
    /* Prevent the stack's periodic report timer from sending a pre-rejoin
     * cached measurement while the new acquisition is in progress. */
    InvalidateAttributes(FRESH_TEMPERATURE | FRESH_FLOW);
    RequestSample();
#endif
}

void APP_ZIGBEE_CancelFreshReports(void)
{
#if APP_IS_SLAVE
    freshPending = 0U;
    freshGeneration = 0U;
    InvalidateAttributes(FRESH_TEMPERATURE | FRESH_FLOW);
#endif
}

void APP_ZIGBEE_ServiceMeasurements(void)
{
#if APP_IS_SLAVE
    if (freshPending == 0U) {
        APP_ZIGBEE_UpdateTemperature();
        APP_ZIGBEE_UpdateFlow();
        return;
    }
    AppStatus status;
    uint32_t now = HAL_GetTick();
    if (!AppIpc_ReadLocalStatus(&status)) { return; }
    if (freshGeneration == 0U ||
        status.latest_sample.refresh_generation != freshGeneration ||
        (uint32_t)(now - status.latest_sample.timestamp_ms) > APP_FLOW_SAMPLE_MAX_AGE_MS) {
        /* Recover a missed event, queue pressure or a binding installed later
         * than the sample lifetime. Never force-send the old cached sample. */
        if (freshGeneration == 0U || (uint32_t)(now - freshRequestMs) >= 5000U) {
            InvalidateAttributes(freshPending);
            RequestSample();
        }
        return;
    }

    bool updated[2] = {
        APP_ZIGBEE_UpdateTemperature(), APP_ZIGBEE_UpdateFlow()
    };
    struct ZbZclClusterT *clusters[2] = {
        stZigbeeAppInfo.temperatureCluster, stZigbeeAppInfo.flowCluster
    };
    const uint16_t ids[2] = {ZCL_CLUSTER_MEAS_TEMPERATURE, APP_ZB_FLOW_CLUSTER_ID};
    const char *names[2] = {"temperature", "flow"};
    for (unsigned int i = 0U; i < 2U; ++i) {
        uint8_t bit = (uint8_t)(1U << i);
        if ((freshPending & bit) == 0U || !updated[i] || clusters[i] == NULL ||
            ZbApsBindSrcCount(stZigbeeAppInfo.pstZigbee, APP_NETWORK_ENDPOINT, ids[i]) == 0U) {
            continue;
        }
        /* true bypasses the periodic/change threshold, even for an unchanged
         * value. API acceptance is scheduling, not proof of Master reception. */
        enum ZclStatusCodeT result = ZbZclClusterReportsSend(clusters[i], true, NULL, NULL);
        if (result == ZCL_STATUS_SUCCESS) {
            freshPending &= (uint8_t)~bit;
            LOG_INFO_APP("[ZB-FRESH SLAVE] %s report requested from new measurement, generation=%lu",
                names[i], (unsigned long)freshGeneration);
        } else {
            LOG_ERROR_APP("[ZB-FRESH SLAVE] %s report request failed: 0x%02X; will retry",
                names[i], (unsigned int)result);
        }
    }
#endif
}
