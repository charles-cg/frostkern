/* Actual SensorTask + IPC + report coordinator/update functions; fake HAL/SDK. */
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "app_ipc.h"
#include "app_temperature_report.h"
#include "app_flow_report.h"
#include "app_network_config.h"
#include "fake_rtos.h"
#include "main.h"
#include "task.h"
#include "log_module.h"
#include "../../Core/Src/sensor_task.c"

ADC_HandleTypeDef hadc4;
LPTIM_HandleTypeDef hlptim1;
static jmp_buf exitTask;
static unsigned int windows,stopAfter,rank,adcReads;
static uint16_t count=65000U;
static uint32_t adcRaw=2844U;
static bool biasOn,boostOn,flowOn,adcFails,requestDuringWindow;
void APP_ZIGBEE_RequestFreshReports(void);
enum ZclStatusCodeT {ZCL_STATUS_SUCCESS=0,ZCL_STATUS_FAILURE=1};
enum ZclDataTypeT {ZCL_DATATYPE_SIGNED_16BIT=0x29};
struct ZbZclClusterT {unsigned id;};
static struct ZbZclClusterT temperatureCluster={0x0402U},flowCluster={0xFC10U};
static unsigned int stack;
static struct {void *pstZigbee;struct ZbZclClusterT *temperatureCluster,*flowCluster;}
    stZigbeeAppInfo={&stack,&temperatureCluster,&flowCluster};
#define APP_ZIGBEE_REPORT_DIAGNOSTICS 0
#define APP_ZB_TEMP_MIN_CENTI_C 100
#define APP_ZB_TEMP_MAX_CENTI_C 10000
#define ZCL_TEMP_MEAS_ATTR_MEAS_VAL 0U
#define ZCL_TEMP_MEAS_UNKNOWN 0x8000U
#define ZCL_CLUSTER_MEAS_TEMPERATURE 0x0402U
static long long attributes[2];
static unsigned int bindings[2]={1U,1U},sent[2];
static bool failTemperatureWrite,failFlowReport;
#define FRESH_TEMPERATURE 1U
#define FRESH_FLOW 2U
static uint8_t freshPending;
static uint32_t freshGeneration,freshRequestMs;

uint32_t HAL_GetTick(void){return Integration_TimeMs;}
void Error_Handler(void){assert(0);}
void MX_ADC4_Init(void){}
void MX_LPTIM1_Init(void){}
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *a)
{assert(a==&hadc4);return HAL_OK;}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a)
{assert(a==&hadc4);return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a)
{assert(a==&hadc4&&biasOn);rank=0U;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *a,uint32_t timeout)
{assert(a==&hadc4&&timeout==10U);++rank;return adcFails?HAL_TIMEOUT:HAL_OK;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a)
{assert(a==&hadc4);++adcReads;return rank==1U?adcRaw:1000U;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *a)
{assert(a==&hadc4);return HAL_OK;}
uint32_t HAL_LPTIM_ReadCounter(LPTIM_HandleTypeDef *h)
{assert(h==&hlptim1);return count;}
HAL_StatusTypeDef HAL_LPTIM_Counter_Start(LPTIM_HandleTypeDef *h)
{assert(h==&hlptim1&&boostOn&&flowOn);return HAL_OK;}
void HAL_GPIO_WritePin(void *port,uint16_t pin,int state)
{
    (void)port;
    if(pin==NTC_BIAS_Pin)biasOn=state==GPIO_PIN_SET;
    else if(pin==BOOST_EN_Pin)boostOn=state==GPIO_PIN_SET;
    else if(pin==YFB7_EN_Pin)flowOn=state==GPIO_PIN_SET;
    else assert(0);
}
void vTaskDelay(TickType_t ticks)
{
    if(ticks==pdMS_TO_TICKS(1000U)){
        if(windows++==stopAfter)longjmp(exitTask,1);
        if(requestDuringWindow) {
            requestDuringWindow=false;
            APP_ZIGBEE_RequestFreshReports();
        }
        count=(uint16_t)(count+22U); // 22 Hz = 2 L/min over a FULL second.
    }
    Integration_TimeMs+=ticks*portTICK_PERIOD_MS;
}
void vTaskDelete(TaskHandle_t task){(void)task;assert(0);}
static void RunOneAcquisition(void)
{
    windows=0U;stopAfter=1U;adcReads=0U;
    if(setjmp(exitTask)==0)SensorTask(NULL);
    assert(!biasOn);
}
static enum ZclStatusCodeT ZbZclAttrIntegerWrite(struct ZbZclClusterT *cluster,
                                               uint16_t attr,long long value)
{
    assert(attr==0U);
    unsigned i=cluster==&temperatureCluster?0U:1U;
    assert(cluster==&temperatureCluster||cluster==&flowCluster);
    if(i==0U&&failTemperatureWrite)return ZCL_STATUS_FAILURE;
    attributes[i]=value;return ZCL_STATUS_SUCCESS;
}
static unsigned int ZbApsBindSrcCount(void *zb,uint8_t endpoint,uint16_t cluster)
{assert(zb==&stack&&endpoint==17U);return bindings[cluster==0x0402U?0U:1U];}
static enum ZclStatusCodeT ZbZclClusterReportsSend(struct ZbZclClusterT *cluster,
    bool all,void (*callback)(struct ZbZclClusterT *,unsigned int,void *),void *arg)
{
    assert(all&&callback==NULL&&arg==NULL);
    unsigned i=cluster==&temperatureCluster?0U:1U;
    AppStatus status;assert(AppIpc_ReadLocalStatus(&status));
    assert(status.latest_sample.refresh_generation==freshGeneration);
    assert(freshGeneration!=0U);
    assert((uint32_t)(HAL_GetTick()-status.latest_sample.timestamp_ms)<=5000U);
    if(i==1U&&failFlowReport)return ZCL_STATUS_FAILURE;
    ++sent[i];return ZCL_STATUS_SUCCESS;
}
#include "fresh_temperature.inc"
#include "fresh_flow.inc"
#include "fresh_request_sample.inc"
#include "fresh_invalidate.inc"
#include "fresh_request_reports.inc"
#include "fresh_cancel.inc"
#include "fresh_service.inc"

int main(void)
{
    AppIpc_Init();Integration_TimeMs=10000U;
    AppSample old={.sequence=50U,.timestamp_ms=HAL_GetTick(),
        .valid=APP_SAMPLE_VALID_TEMP|APP_SAMPLE_VALID_FLOW,
        .temperature_centi_c=2000,.flow_ml_min=9999U};
    assert(AppIpc_PublishSample(&old));
    attributes[0]=2000;attributes[1]=9999U;
    uint32_t requestAt=HAL_GetTick();APP_ZIGBEE_RequestFreshReports();
    uint32_t first=freshGeneration;
    assert(first!=0U&&attributes[0]==INT16_MIN&&attributes[1]==UINT32_MAX);
    // A sample captured before the request but published after it is rejected.
    old.timestamp_ms+=10U;old.sequence++;
    assert(AppIpc_PublishSample(&old));APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==0U&&sent[1]==0U);
    RunOneAcquisition();AppStatus current;
    assert(AppIpc_ReadLocalStatus(&current));
    assert(current.latest_sample.refresh_generation==first);
    assert((uint32_t)(current.latest_sample.timestamp_ms-requestAt)>=1000U);
    assert(current.latest_sample.flow_ml_min==2000U);
    assert(current.latest_sample.temperature_centi_c>=2498&&current.latest_sample.temperature_centi_c<=2502);
    assert(adcReads==2U);
    // Real IPC wakeup is present and Control's one-slot queue still has the sample.
    AppZbCommand wake;assert(xQueueReceive(qZbCommand,&wake,0U)==pdPASS);
    assert(wake.type==APP_ZB_COMMAND_SAMPLE_READY);
    AppSample control;assert(xQueueReceive(qSample,&control,0U)==pdPASS);
    assert(control.refresh_generation==first);
    APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==1U&&sent[1]==1U&&freshPending==0U);
    assert(attributes[0]==current.latest_sample.temperature_centi_c&&attributes[1]==2000U);
    APP_ZIGBEE_ServiceMeasurements();assert(sent[0]==1U&&sent[1]==1U);

    // Initial join: one binding may arrive much later than the other.
    bindings[0]=bindings[1]=0U;
    APP_ZIGBEE_RequestFreshReports();RunOneAcquisition();
    APP_ZIGBEE_ServiceMeasurements();assert(freshPending==3U);
    Integration_TimeMs+=6000U;bindings[0]=bindings[1]=1U;
    APP_ZIGBEE_ServiceMeasurements();uint32_t second=freshGeneration;
    assert(second!=first&&sent[0]==1U&&sent[1]==1U);
    // No forced report from the expired generation while new acquisition runs.
    APP_ZIGBEE_ServiceMeasurements();assert(sent[0]==1U);
    RunOneAcquisition();failFlowReport=true;APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==2U&&sent[1]==1U&&freshPending==FRESH_FLOW);
    failFlowReport=false;APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==2U&&sent[1]==2U&&freshPending==0U);

    // Unchanged readings must STILL be forced on another reconnect.
    APP_ZIGBEE_RequestFreshReports();RunOneAcquisition();
    failTemperatureWrite=true;APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==2U&&sent[1]==3U&&freshPending==FRESH_TEMPERATURE);
    failTemperatureWrite=false;APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==3U&&sent[1]==3U);

    // A newly measured ADC failure reports unknown, never the previous value.
    APP_ZIGBEE_RequestFreshReports();adcFails=true;RunOneAcquisition();
    APP_ZIGBEE_ServiceMeasurements();assert(attributes[0]==INT16_MIN&&sent[0]==4U);
    adcFails=false;

    APP_ZIGBEE_RequestFreshReports();APP_ZIGBEE_CancelFreshReports();
    assert(freshPending==0U&&attributes[0]==INT16_MIN&&attributes[1]==UINT32_MAX);
    unsigned before=sent[0];RunOneAcquisition();
    APP_ZIGBEE_ServiceMeasurements();assert(sent[0]==before);

    Integration_MutexAllowed=false;APP_ZIGBEE_RequestFreshReports();
    assert(freshGeneration==0U);Integration_MutexAllowed=true;
    APP_ZIGBEE_ServiceMeasurements();assert(freshGeneration!=0U);
    RunOneAcquisition();APP_ZIGBEE_ServiceMeasurements();assert(freshPending==0U);

    // A second reconnect during the full flow window supersedes the first.
    xQueueReset(qZbCommand);
    APP_ZIGBEE_RequestFreshReports();uint32_t earlier=freshGeneration;
    requestDuringWindow=true;RunOneAcquisition();
    assert(freshGeneration!=earlier);
    assert(AppIpc_ReadLocalStatus(&current)&&current.latest_sample.refresh_generation==earlier);
    unsigned countBefore=sent[0];APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==countBefore);
    // The single-window harness interrupted SensorTask at the start of its
    // second window, after consuming that event. Re-arm the outstanding
    // request to emulate resuming acquisition instead of losing its stack.
    xEventGroupSetBits(evSystem,APP_EVENT_SAMPLE_REQUEST);
    RunOneAcquisition();APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==countBefore+1U&&freshPending==0U);

    // Even a full Zigbee command queue cannot lose the protected fresh sample.
    xQueueReset(qZbCommand);
    APP_ZIGBEE_RequestFreshReports();
    AppZbCommand dummy={.type=APP_ZB_COMMAND_SAMPLE_READY};
    for(unsigned i=0U;i<16U;++i)assert(xQueueSend(qZbCommand,&dummy,0U)==pdPASS);
    RunOneAcquisition();
    assert(AppIpc_ReadLocalStatus(&current)&&current.latest_sample.refresh_generation==freshGeneration);
    APP_ZIGBEE_ServiceMeasurements();assert(freshPending==0U);

    // Each stream completes independently; delayed flow binding must not
    // clear a temperature report already refreshed successfully.
    xQueueReset(qZbCommand);bindings[0]=1U;bindings[1]=0U;
    unsigned oldT=sent[0],oldF=sent[1];
    APP_ZIGBEE_RequestFreshReports();RunOneAcquisition();APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==oldT+1U&&sent[1]==oldF&&freshPending==FRESH_FLOW);
    long long currentT=attributes[0];
    Integration_TimeMs+=6000U;APP_ZIGBEE_ServiceMeasurements();
    assert(attributes[0]==currentT&&attributes[1]==UINT32_MAX);
    bindings[1]=1U;RunOneAcquisition();APP_ZIGBEE_ServiceMeasurements();
    assert(sent[0]==oldT+1U&&sent[1]==oldF+1U&&freshPending==0U);
    Integration_FreeObjects();
    puts("PASS fresh reconnect: real ADC and full flow window, generation handshake, late pre-request sample, independent bindings, stale samples, unchanged readings, failed writes/send, invalid sensor and IPC wakeup.");
}
