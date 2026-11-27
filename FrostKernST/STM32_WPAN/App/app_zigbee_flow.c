/* FrostKern private flow cluster. Keeps the tested temperature path intact. */
#include <stdint.h>
#include <stdbool.h>
#include "app_common.h"
#include "main.h"
#include "app_zigbee.h"
#include "app_ipc.h"
#include "app_network_config.h"
#include "app_flow_report.h"
#include "app_zb_report_retry.h"
#include "log_module.h"
#include "task.h"
#include "zigbee.aps.h"
#include "zigbee.nwk.h"
#include "zcl/zcl.h"
#define APP_ZIGBEE_ENDPOINT APP_NETWORK_ENDPOINT
#if APP_IS_MASTER

_Static_assert(APP_SLAVE_COUNT == APP_ZB_REPORT_RETRY_PEERS,
               "Report retry slots must match the Slave count");
_Static_assert(sizeof(uintptr_t) >= sizeof(uint32_t),
               "Callback ticket must fit in uintptr_t");

typedef struct
{
    uint32_t token;
    uint32_t aps_status;
    uint32_t zcl_status;
    uint8_t attribute_status;
    bool accepted;
} AppFlowReportConfigResult;

static AppZbReportRetry flowReportRetries;
static QueueHandle_t flowReportResults = NULL;

#endif

#if APP_IS_MASTER
static void APP_ZIGBEE_FlowReport(struct ZbZclClusterT *cluster,
    struct ZbZclHeaderT *header, struct ZbApsdeDataIndT *data_ind,
    uint16_t attribute_id, enum ZclDataTypeT data_type,
    const uint8_t *payload, uint16_t length, bool *discard)
{
    (void)cluster; (void)header; (void)discard;
    if (data_ind == NULL || payload == NULL || qZbCommand == NULL ||
        data_ind->src.endpoint != APP_NETWORK_ENDPOINT ||
        data_ind->dst.endpoint != APP_NETWORK_ENDPOINT ||
        attribute_id != APP_ZB_FLOW_ATTRIBUTE_ID ||
        data_type != ZCL_DATATYPE_UNSIGNED_32BIT || length < 4U) {
        return;
    }
    /* in_len can include subsequent records, as in the temperature fix. */
    uint32_t value = (uint32_t)payload[0] | ((uint32_t)payload[1] << 8U) |
        ((uint32_t)payload[2] << 16U) | ((uint32_t)payload[3] << 24U);
    AppZbCommand command = {0};
    command.type = APP_ZB_COMMAND_FLOW_RECEIVED;
    command.flow_ml_min = value;
    command.flow_valid = value != APP_ZB_FLOW_UNKNOWN;
    command.short_address = 0xFFFFU;
    if (data_ind->src.mode == ZB_APSDE_ADDRMODE_EXT) {
        command.ieee_address = data_ind->src.extAddr;
    } else if (data_ind->src.mode == ZB_APSDE_ADDRMODE_SHORT) {
        command.short_address = data_ind->src.nwkAddr;
    } else { return; }
    if (xQueueSend(qZbCommand, &command, 0U) != pdPASS) {
        LOG_ERROR_APP("Flow report dropped: command queue full");
    }
}
#endif

void APP_ZIGBEE_ConfigFlowEndpoint(void)
{
#if APP_IS_MASTER
    enum ZbZclDirectionT direction = ZCL_DIRECTION_TO_CLIENT;
#else
    enum ZbZclDirectionT direction = ZCL_DIRECTION_TO_SERVER;
#endif
    struct ZbZclClusterT *cluster = ZbZclClusterAlloc(
        stZigbeeAppInfo.pstZigbee, sizeof(struct ZbZclClusterT),
        (enum ZbZclClusterIdT)APP_ZB_FLOW_CLUSTER_ID, APP_NETWORK_ENDPOINT, direction);
    if (cluster == NULL) { Error_Handler(); return; }
    stZigbeeAppInfo.flowCluster = cluster;
#if APP_IS_SLAVE
    /* Static lifetime: ZbZclAttrAppendList retains this descriptor. The measured
     * value is reportable but not persistable: never wear Flash per sample. */
    static const struct ZbZclAttrT attrs[] = {{
        .attributeId = APP_ZB_FLOW_ATTRIBUTE_ID,
        .dataType = ZCL_DATATYPE_UNSIGNED_32BIT,
        .flags = ZCL_ATTR_FLAG_REPORTABLE,
        .reporting = {.interval_min = 60U, .interval_max = 300U}
    }};
    if (ZbZclAttrAppendList(cluster, attrs, ZCL_ATTR_LIST_LEN(attrs)) != ZCL_STATUS_SUCCESS ||
        ZbZclAttrIntegerWrite(cluster, APP_ZB_FLOW_ATTRIBUTE_ID,
                             APP_ZB_FLOW_UNKNOWN) != ZCL_STATUS_SUCCESS) {
        ZbZclClusterFree(cluster); stZigbeeAppInfo.flowCluster = NULL;
        Error_Handler(); return;
    }
#else
    cluster->report = APP_ZIGBEE_FlowReport;
#endif
    if (ZbZclClusterAttach(cluster) != ZCL_STATUS_SUCCESS ||
        !ZbZclClusterEndpointRegister(cluster)) {
        ZbZclClusterFree(cluster); stZigbeeAppInfo.flowCluster = NULL;
        Error_Handler(); return;
    }
    LOG_INFO_APP("FrostKern flow cluster 0xFC10 on endpoint %u (mL/min)",
                 (unsigned int)APP_NETWORK_ENDPOINT);
}

bool APP_ZIGBEE_UpdateFlow(void)
{
#if APP_IS_SLAVE
    AppStatus localStatus;
    if (stZigbeeAppInfo.flowCluster == NULL || !AppIpc_ReadLocalStatus(&localStatus)) {
        return false;
    }
    uint32_t value = AppFlowReport_Value(&localStatus.latest_sample, HAL_GetTick());
    enum ZclStatusCodeT status = ZbZclAttrIntegerWrite(
        stZigbeeAppInfo.flowCluster, APP_ZB_FLOW_ATTRIBUTE_ID, value);
    if (status != ZCL_STATUS_SUCCESS) {
        LOG_ERROR_APP("Flow attribute update failed: 0x%02X", (unsigned int)status);
    }
    static bool logged;
    static uint32_t lastLog;
    uint32_t now = HAL_GetTick();
    if (!logged || (uint32_t)(now - lastLog) >= 10000U) {
        logged = true; lastLog = now;
        unsigned int bindings = ZbApsBindSrcCount(stZigbeeAppInfo.pstZigbee,
            APP_NETWORK_ENDPOINT, APP_ZB_FLOW_CLUSTER_ID);
        if (value == APP_ZB_FLOW_UNKNOWN) {
            LOG_INFO_APP("[ZB-FLOW SLAVE] Q=unavailable bindings=%u", bindings);
        } else {
            LOG_INFO_APP("[ZB-FLOW SLAVE] Q=%lu mL/min bindings=%u",
                (unsigned long)value, bindings);
        }
    }
    return status == ZCL_STATUS_SUCCESS;
#else
    return false;
#endif
}

#if APP_IS_MASTER

static uint32_t APP_ZIGBEE_ReportRetryNowMs(void)
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

static void APP_ZIGBEE_FlowReportConfigCallback(
    struct ZbZclCommandRspT *response,
    void *argument)
{
    /* The argument is a request ticket, never a pointer to a stack variable. */
    AppFlowReportConfigResult result = {0};
    result.token = (uint32_t)(uintptr_t)argument;
    result.aps_status = response != NULL
        ? (uint32_t)response->aps_status : UINT32_MAX;
    result.zcl_status = response != NULL
        ? (uint32_t)response->status : UINT32_MAX;
    result.attribute_status = 0xFFU;

    if (response != NULL && response->payload != NULL && response->length > 0U)
    {
        result.attribute_status = response->payload[0];
    }

    result.accepted =
        response != NULL &&
        response->aps_status == ZB_STATUS_SUCCESS &&
        response->status == ZCL_STATUS_SUCCESS &&
        response->hdr.cmdId == ZCL_COMMAND_CONFIG_REPORTING_RESPONSE &&
        response->payload != NULL &&
        response->length == 1U &&
        response->payload[0] == ZCL_STATUS_SUCCESS;

    /* Callback runs in the ST stack task. Do not call Zigbee APIs or wait here. */
    if (flowReportResults == NULL ||
        xQueueSend(flowReportResults, &result, 0U) != pdPASS)
    {
        LOG_ERROR_APP("Flow config response queue full; deadline will recover");
    }
}

static enum ZclStatusCodeT APP_ZIGBEE_SendFlowReportConfig(
    uint16_t shortAddress,
    uint32_t token)
{
    if (stZigbeeAppInfo.flowCluster == NULL)
    {
        return ZCL_STATUS_FAILURE;
    }

    struct ZbZclAttrReportConfigT config = {0};
    config.dst.mode = ZB_APSDE_ADDRMODE_SHORT;
    config.dst.endpoint = APP_ZIGBEE_ENDPOINT;
    config.dst.nwkAddr = shortAddress;
    config.num_records = 1U;
    config.record_list[0].direction = ZCL_REPORT_DIRECTION_NORMAL;
    config.record_list[0].attr_id = APP_ZB_FLOW_ATTRIBUTE_ID;
    config.record_list[0].attr_type = ZCL_DATATYPE_UNSIGNED_32BIT;
    config.record_list[0].min = 2U;
    config.record_list[0].max = 10U;
    config.record_list[0].change = 100.0;

    return ZbZclAttrReportConfigReq(
        stZigbeeAppInfo.flowCluster,
        &config,
        APP_ZIGBEE_FlowReportConfigCallback,
        (void *)(uintptr_t)token);
}

#endif

void APP_ZIGBEE_ConfigureFlowReports(
    uint8_t nodeId,
    uint16_t shortAddress)
{
#if APP_IS_MASTER
    if (nodeId == 0U || nodeId > APP_SLAVE_COUNT ||
        shortAddress == 0U || shortAddress >= 0xFFF8U)
    {
        LOG_ERROR_APP("Invalid flow report config destination");
        return;
    }

    if (flowReportResults == NULL)
    {
        flowReportResults = xQueueCreate(
            8U, sizeof(AppFlowReportConfigResult));
        if (flowReportResults == NULL)
        {
            Error_Handler();
            return;
        }
        AppZbReportRetry_Init(&flowReportRetries);
    }

    if (AppZbReportRetry_Schedule(&flowReportRetries,
            nodeId, shortAddress, APP_ZIGBEE_ReportRetryNowMs()))
    {
        LOG_INFO_APP("Slave %u: flow report config scheduled in 20 s",
                     (unsigned int)nodeId);
    }
#else
    (void)nodeId;
    (void)shortAddress;
#endif
}

void APP_ZIGBEE_ProcessFlowReportRetries(void)
{
#if APP_IS_MASTER
    if (flowReportResults == NULL)
    {
        return;
    }

    AppFlowReportConfigResult result;
    while (xQueueReceive(flowReportResults, &result, 0U) == pdPASS)
    {
        uint8_t nodeId = AppZbReportRetry_Complete(
            &flowReportRetries, result.token, result.accepted,
            APP_ZIGBEE_ReportRetryNowMs());
        if (nodeId == 0U)
        {
            continue; /* Old response from a previous attempt or network address. */
        }
        if (result.accepted)
        {
            LOG_INFO_APP("Slave %u: flow report configuration accepted",
                         (unsigned int)nodeId);
        }
        else
        {
            LOG_ERROR_APP("Slave %u: flow report config response failed, APS=%lu ZCL=%lu Attr=%u",
                          (unsigned int)nodeId, (unsigned long)result.aps_status,
                          (unsigned long)result.zcl_status,
                          (unsigned int)result.attribute_status);
        }
    }

    AppZbReportRetryAction action;
    while (AppZbReportRetry_Next(&flowReportRetries,
                                APP_ZIGBEE_ReportRetryNowMs(), &action))
    {
        if (action.type == APP_ZB_REPORT_ACTION_TIMEOUT)
        {
            LOG_ERROR_APP("Slave %u: flow report config attempt %u timed out",
                          (unsigned int)action.node_id,
                          (unsigned int)action.attempt);
        }
        else if (action.type == APP_ZB_REPORT_ACTION_EXHAUSTED)
        {
            LOG_ERROR_APP("Slave %u: flow report config stopped after 3 attempts",
                          (unsigned int)action.node_id);
        }
        else
        {
            LOG_INFO_APP("Slave %u: flow report config attempt %u/3 to 0x%04X",
                         (unsigned int)action.node_id,
                         (unsigned int)action.attempt,
                         (unsigned int)action.short_address);

            enum ZclStatusCodeT status = APP_ZIGBEE_SendFlowReportConfig(
                action.short_address, action.token);

            if (status != ZCL_STATUS_SUCCESS)
            {
                LOG_ERROR_APP("Slave %u: flow report config could not be sent, status=%u",
                              (unsigned int)action.node_id,
                              (unsigned int)status);
                (void)AppZbReportRetry_Complete(&flowReportRetries,
                    action.token, false, APP_ZIGBEE_ReportRetryNowMs());
            }
        }
    }
#endif
}

/* Restored devices may retain the temperature binding but not the newly added
 * flow binding. Reconfigure missing streams periodically without erasing NVM. */
void APP_ZIGBEE_ProcessReportRecovery(void)
{
#if APP_IS_MASTER
    static bool checked;
    static uint32_t lastCheck;
    uint32_t now = (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
    if (!AppIpc_NetworkStarted() || (!checked && now < 60000U) ||
        (checked && (uint32_t)(now - lastCheck) < 120000U)) {
        return;
    }
    checked = true; lastCheck = now;
    for (uint8_t id = 1U; id <= APP_SLAVE_COUNT; ++id) {
        AppStatus status;
        if (!AppIpc_ReadSlaveStatus(id, &status)) { continue; }
        uint64_t ieee = 0U;
        for (unsigned i = 0U; i < 8U; ++i) {
            ieee = (ieee << 8U) | status.device.ieee_address[i];
        }
        if (ieee == 0U || ieee == UINT64_MAX) { continue; }
        uint16_t address = ZbNwkAddrLookupNwk(stZigbeeAppInfo.pstZigbee, ieee);
        if (address == 0U || address >= 0xFFF8U) { continue; }
        if ((status.reports_seen & APP_SAMPLE_VALID_TEMP) == 0U ||
            (uint32_t)(now - status.temperature_report_at_ms) > 180000U) {
            APP_ZIGBEE_ConfigureTemperatureReports(id, address);
        }
        if ((status.reports_seen & APP_SAMPLE_VALID_FLOW) == 0U ||
            (uint32_t)(now - status.flow_report_at_ms) > 30000U) {
            APP_ZIGBEE_ConfigureFlowReports(id, address);
        }
    }
#endif
}
