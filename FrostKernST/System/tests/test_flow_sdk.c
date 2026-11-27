/* Production endpoint/update/config snippets against fake SDK services. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_ipc.h"
#include "app_flow_report.h"
#include "app_network_config.h"
#include "app_zb_report_retry.h"
#include "task.h"
#include "main.h"
#include "fake_rtos.h"
#include "log_module.h"
#define APP_ZIGBEE_ENDPOINT APP_NETWORK_ENDPOINT
enum ZclStatusCodeT {ZCL_STATUS_SUCCESS=0,ZCL_STATUS_FAILURE=1};
enum ZclDataTypeT {ZCL_DATATYPE_UNSIGNED_32BIT=0x23};
enum ZbZclDirectionT {ZCL_DIRECTION_TO_SERVER,ZCL_DIRECTION_TO_CLIENT};
enum ZbZclClusterIdT {TEST_CLUSTER=0xFC10};
#define ZCL_ATTR_FLAG_REPORTABLE 2U
#define ZCL_ATTR_LIST_LEN(a) (sizeof(a)/sizeof(a[0]))
#define ZB_STATUS_SUCCESS 0U
#define ZCL_COMMAND_CONFIG_REPORTING_RESPONSE 7U
#define ZB_APSDE_ADDRMODE_SHORT 2U
#define ZB_APSDE_ADDRMODE_EXT 3U
#define ZCL_REPORT_DIRECTION_NORMAL 0U
struct ZbZclHeaderT {unsigned int cmdId;};
struct ZbApsdeDataIndT {
    struct {unsigned int endpoint;} dst;
    struct {unsigned int endpoint,mode;uint64_t extAddr;uint16_t nwkAddr;} src;
};
struct ZbZclClusterT {
    void (*report)(struct ZbZclClusterT *,struct ZbZclHeaderT *,struct ZbApsdeDataIndT *,
                   uint16_t,enum ZclDataTypeT,const uint8_t *,uint16_t,bool *);
};
struct ZbZclAttrT {uint16_t attributeId;enum ZclDataTypeT dataType;unsigned int flags;
                  struct {unsigned int interval_min,interval_max;} reporting;};
struct ZbZclAttrReportConfigT {
    struct {unsigned mode,endpoint;uint16_t nwkAddr;} dst;
    unsigned int num_records;
    struct {unsigned direction,attr_id,attr_type,min,max;double change;} record_list[1];
};
struct ZbZclCommandRspT {
    uint32_t aps_status,status;struct ZbZclHeaderT hdr;uint8_t *payload;unsigned length;
};
static struct ZbZclClusterT cluster;
static int stack;
static struct {void *pstZigbee;struct ZbZclClusterT *flowCluster;} stZigbeeAppInfo={&stack,NULL};
static uint32_t stored;
static unsigned int writes,allocated,attached,registered;
static void *ZbZclClusterAlloc(void *zb,unsigned int size,enum ZbZclClusterIdT id,
                              unsigned int endpoint,enum ZbZclDirectionT direction)
{
    assert(zb==&stack&&size==sizeof(cluster)&&id==TEST_CLUSTER&&endpoint==17U);
    assert(direction==(APP_IS_MASTER?ZCL_DIRECTION_TO_CLIENT:ZCL_DIRECTION_TO_SERVER));
    ++allocated;return &cluster;
}
static enum ZclStatusCodeT ZbZclClusterAttach(struct ZbZclClusterT *c)
{assert(c==&cluster);++attached;return ZCL_STATUS_SUCCESS;}
static bool ZbZclClusterEndpointRegister(struct ZbZclClusterT *c)
{assert(c==&cluster);++registered;return true;}
static void ZbZclClusterFree(struct ZbZclClusterT *c){(void)c;assert(0);}
#if APP_IS_SLAVE
static unsigned int ZbApsBindSrcCount(void *zb,uint8_t endpoint,uint16_t id)
{assert(zb==&stack&&endpoint==17U&&id==APP_ZB_FLOW_CLUSTER_ID);return 1U;}
static enum ZclStatusCodeT ZbZclAttrAppendList(struct ZbZclClusterT *c,
    const struct ZbZclAttrT *attrs,unsigned int count)
{
    assert(c==&cluster&&count==1U&&attrs[0].attributeId==0U);
    assert(attrs[0].dataType==0x23&&attrs[0].flags==2U);
    assert(attrs[0].reporting.interval_min==60U&&attrs[0].reporting.interval_max==300U);
    return ZCL_STATUS_SUCCESS;
}
static enum ZclStatusCodeT ZbZclAttrIntegerWrite(struct ZbZclClusterT *c,uint16_t id,
                                               unsigned long long value)
{assert(c==&cluster&&id==0U);stored=(uint32_t)value;++writes;return ZCL_STATUS_SUCCESS;}
#else
typedef struct {uint32_t token,aps_status,zcl_status;uint8_t attribute_status;bool accepted;}
    AppFlowReportConfigResult;
static AppZbReportRetry flowReportRetries;
static QueueHandle_t flowReportResults;
static void (*configCallback)(struct ZbZclCommandRspT *,void *);
static void *configArgument;
static unsigned int configRequests;
static unsigned int tempConfigRequests;
static uint16_t ZbNwkAddrLookupNwk(void *zb,uint64_t ieee)
{assert(zb==&stack);return ieee==UINT64_C(0x00124B000ABCDEF0)?0x1234U:0xFFFFU;}
static void APP_ZIGBEE_ConfigureTemperatureReports(uint8_t id,uint16_t address)
{assert(id==1U&&address==0x1234U);++tempConfigRequests;}
static enum ZclStatusCodeT ZbZclAttrReportConfigReq(struct ZbZclClusterT *c,
    struct ZbZclAttrReportConfigT *config,void (*fn)(struct ZbZclCommandRspT *,void *),void *arg)
{
    assert(c==&cluster&&config->dst.mode==2U&&config->dst.endpoint==17U);
    assert(config->dst.nwkAddr==0x1234U&&config->num_records==1U);
    assert(config->record_list[0].attr_id==0U&&config->record_list[0].attr_type==0x23);
    assert(config->record_list[0].min==2U&&config->record_list[0].max==10U);
    assert(config->record_list[0].change==100.0);
    configCallback=fn;configArgument=arg;++configRequests;return ZCL_STATUS_SUCCESS;
}
#include "flow_callback.inc"
#include "flow_now.inc"
#include "flow_config_callback.inc"
#include "flow_send_config.inc"
#endif
void Error_Handler(void){assert(0);}
uint32_t HAL_GetTick(void){return Integration_TimeMs;}
#include "flow_endpoint.inc"
#include "flow_update.inc"
#include "flow_configure.inc"
#include "flow_retries.inc"
#include "flow_recovery.inc"
int main(void)
{
    AppIpc_Init();APP_ZIGBEE_ConfigFlowEndpoint();
    assert(allocated==1U&&attached==1U&&registered==1U);
#if APP_IS_SLAVE
    assert(stored==UINT32_MAX&&writes==1U);
    Integration_TimeMs=10000U;
    AppSample sample={.valid=APP_SAMPLE_VALID_FLOW,.flow_ml_min=70000U,.timestamp_ms=10000U};
    assert(AppIpc_PublishSample(&sample));APP_ZIGBEE_UpdateFlow();assert(stored==70000U);
    sample.flow_ml_min=0U;assert(AppIpc_PublishSample(&sample));
    APP_ZIGBEE_UpdateFlow();assert(stored==0U);
    Integration_TimeMs+=5001U;APP_ZIGBEE_UpdateFlow();assert(stored==UINT32_MAX);
#else
    (void)stored;(void)writes;
    APP_ZIGBEE_ConfigureFlowReports(1U,0x1234U);
    APP_ZIGBEE_ProcessFlowReportRetries();assert(configRequests==0U);
    Integration_TimeMs=20000U;APP_ZIGBEE_ProcessFlowReportRetries();assert(configRequests==1U);
    uint8_t accepted=0U;
    struct ZbZclCommandRspT response={.hdr={7U},.payload=&accepted,.length=1U};
    configCallback(&response,configArgument);APP_ZIGBEE_ProcessFlowReportRetries();
    assert(flowReportRetries.peers[0].state==APP_ZB_REPORT_CONFIRMED);
    APP_ZIGBEE_ConfigureFlowReports(2U,0x1234U);
    Integration_TimeMs+=20000U;APP_ZIGBEE_ProcessFlowReportRetries();
    response.length=0U;configCallback(&response,configArgument);
    APP_ZIGBEE_ProcessFlowReportRetries();
    assert(flowReportRetries.peers[1].state==APP_ZB_REPORT_WAITING);
    assert(AppIpc_RegisterSlave(UINT64_C(0x00124B000ABCDEF0))==1U);
    AppIpc_MarkNetworkStarted();Integration_TimeMs=60000U;
    APP_ZIGBEE_ProcessReportRecovery();assert(tempConfigRequests==1U);
    assert(flowReportRetries.peers[0].state==APP_ZB_REPORT_WAITING);
    assert(AppIpc_SaveSlaveTemperature(UINT64_C(0x00124B000ABCDEF0),2500,true));
    Integration_TimeMs=180000U;APP_ZIGBEE_ProcessReportRecovery();
    assert(tempConfigRequests==1U); // Fresh temperature does not need configuring.
#endif
    Integration_FreeObjects();
    puts("PASS production flow endpoint, type/units, initial unknown, sample writes/config and independent retry policy (fake SDK).");
}
