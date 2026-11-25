#include "app_temperature_report.h"
#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define APP_IS_SLAVE 1
#define APP_IS_MASTER 0
#define APP_ZIGBEE_REPORT_DIAGNOSTICS 1
#define APP_ZIGBEE_ENDPOINT 17U
#define APP_ZB_TEMP_MIN_CENTI_C 100
#define APP_ZB_TEMP_MAX_CENTI_C 10000
#define ZCL_TEMP_MEAS_ATTR_MEAS_VAL 0U
#define ZCL_TEMP_MEAS_UNKNOWN 0x8000U
#define ZCL_CLUSTER_MEAS_TEMPERATURE 0x0402U
#define UNUSED(x) (void)(x)

enum ZclDataTypeT {ZCL_DATATYPE_SIGNED_16BIT=0x29};
enum ZclStatusCodeT {ZCL_STATUS_SUCCESS=0, ZCL_STATUS_FAILURE=1};
struct ZbZclClusterT {unsigned int unused;};
static struct ZbZclClusterT cluster;
static unsigned int stack;
static struct {struct ZbZclClusterT *temperatureCluster; void *pstZigbee;}
    stZigbeeAppInfo={&cluster,&stack};
static AppStatus snapshot;
static uint32_t nowMs;
static bool snapshotAvailable=true;
static int16_t stored;
static unsigned int writes,reads,reportableReads,bindReads,diagLines;
static enum ZclStatusCodeT writeStatus=ZCL_STATUS_SUCCESS;
static unsigned int bindingCount;
static char lastDiagnostic[256];

static void TestLog(const char *format, ...) __attribute__((format(printf,1,2)));
static void TestLog(const char *format, ...)
{
    char line[256]; va_list args; va_start(args,format);
    int size=vsnprintf(line,sizeof(line),format,args); va_end(args);
    assert(size>=0 && (size_t)size<sizeof(line));
    if (strstr(line,"[ZB-DIAG SLAVE]")!=NULL) {
        ++diagLines;strcpy(lastDiagnostic,line);
    }
}
#define LOG_INFO_APP(...) TestLog(__VA_ARGS__)
#define LOG_ERROR_APP(...) TestLog(__VA_ARGS__)
static uint32_t HAL_GetTick(void){return nowMs;}
static bool AppIpc_ReadLocalStatus(AppStatus *out)
{if(!snapshotAvailable)return false;*out=snapshot;return true;}
static enum ZclStatusCodeT ZbZclAttrIntegerWrite(
    struct ZbZclClusterT *c,uint16_t id,long long value)
{
    assert(c==&cluster&&id==0U);++writes;
    if(writeStatus==ZCL_STATUS_SUCCESS)stored=(int16_t)value;
    return writeStatus;
}
static long long ZbZclAttrIntegerRead(struct ZbZclClusterT *c,uint16_t id,
    enum ZclDataTypeT *type,enum ZclStatusCodeT *status)
{
    assert(c==&cluster&&id==0U);++reads;
    *type=ZCL_DATATYPE_SIGNED_16BIT;*status=ZCL_STATUS_SUCCESS;return stored;
}
static enum ZclStatusCodeT ZbZclAttrRead(struct ZbZclClusterT *c,uint16_t id,
    enum ZclDataTypeT *type,void *out,unsigned int size,bool reporting)
{
    assert(c==&cluster&&id==0U&&type==NULL&&size==2U&&reporting);
    ++reportableReads;memcpy(out,&stored,2U);return ZCL_STATUS_SUCCESS;
}
static unsigned int ZbApsBindSrcCount(void *zb,uint8_t endpoint,uint16_t id)
{assert(zb==&stack&&endpoint==17U&&id==0x0402U);++bindReads;return bindingCount;}

#include "zigbee_diagnostic_update.inc"
#include "zigbee_diagnostic_confirm.inc"

int main(void)
{
    snapshot.latest_sample.valid=APP_SAMPLE_VALID_TEMP;
    snapshot.latest_sample.temperature_centi_c=2361;
    snapshot.latest_sample.timestamp_ms=100U;nowMs=100U;
    AppStatus original=snapshot;
    APP_ZIGBEE_UpdateTemperature();
    assert(stored==2361&&writes==1U&&reads==1U&&reportableReads==1U&&bindReads==1U);
    assert(diagLines==1U&&strstr(lastDiagnostic,"sample=2361 zcl=2361 quality=1")!=NULL);
    assert(memcmp(&original,&snapshot,sizeof(snapshot))==0);
    nowMs=2100U;snapshot.latest_sample.timestamp_ms=nowMs;
    APP_ZIGBEE_UpdateTemperature();
    assert(writes==2U&&reads==1U&&diagLines==1U);
    nowMs=10100U;snapshot.latest_sample.timestamp_ms=nowMs;bindingCount=1U;
    APP_ZIGBEE_UpdateTemperature();
    assert(diagLines==2U&&strstr(lastDiagnostic,"bindings=1")!=NULL);
    nowMs=20100U;snapshot.latest_sample.timestamp_ms=nowMs;writeStatus=ZCL_STATUS_FAILURE;
    APP_ZIGBEE_UpdateTemperature();
    assert(strstr(lastDiagnostic,"write=0x01")!=NULL&&stored==2361);
    nowMs=30100U;snapshotAvailable=false;
    APP_ZIGBEE_UpdateTemperature();
    assert(writes==4U&&strstr(lastDiagnostic,"snapshot unavailable")!=NULL);
    nowMs=40100U;snapshotAvailable=true;writeStatus=ZCL_STATUS_SUCCESS;
    APP_ZIGBEE_UpdateTemperature();
    assert(stored==INT16_MIN&&strstr(lastDiagnostic,"quality=2")!=NULL);
    nowMs=50100U;stZigbeeAppInfo.temperatureCluster=NULL;
    APP_ZIGBEE_UpdateTemperature();assert(writes==5U);
    APP_ZIGBEE_ReportSentDiagnostic(&cluster);
    puts("PASS diagnostics: real update function, read-only probes, unchanged sample, rate limit, failure and stale handling.");
    return 0;
}
