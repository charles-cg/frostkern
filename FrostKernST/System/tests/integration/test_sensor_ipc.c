#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "app_ipc.h"
#include "app_temperature_report.h"
#include "fake_rtos.h"
#include "main.h"
#include "log_module.h"

/* Include acquisition unchanged so its static task can run against fake HAL. */
#include "../../Core/Src/sensor_task.c"

ADC_HandleTypeDef hadc4;
LPTIM_HandleTypeDef hlptim1;
static jmp_buf taskExit;
static unsigned int loops,stopAfter,pollIndex,adcReads,stopCalls;
static uint32_t adcRaw=2844U;
static uint16_t flowCounter=65530U;
static bool biasOn,boostOn,flowPowerOn;
static unsigned int timeoutRank;
static int16_t writtenAttribute;
static unsigned int attributeWrites;

enum ZclStatusCodeT {ZCL_STATUS_SUCCESS=0,ZCL_STATUS_FAILURE=1};
#define ZCL_TEMP_MEAS_UNKNOWN 0x8000U
#define ZCL_TEMP_MEAS_ATTR_MEAS_VAL 0U
#define APP_ZB_TEMP_MIN_CENTI_C 100
#define APP_ZB_TEMP_MAX_CENTI_C 10000
static int cluster;
static struct {void *temperatureCluster;} stZigbeeAppInfo={&cluster};
static enum ZclStatusCodeT ZbZclAttrIntegerWrite(void *c,uint16_t id,long long value)
{assert(c==&cluster&&id==0U);writtenAttribute=(int16_t)value;++attributeWrites;return ZCL_STATUS_SUCCESS;}
#include "zigbee_temperature_update.inc"

uint32_t HAL_GetTick(void){return Integration_TimeMs;}
void Error_Handler(void){assert(0);}
void MX_ADC4_Init(void){hadc4.Init.LowPowerAutoWait=0U;}
void MX_LPTIM1_Init(void){}
HAL_StatusTypeDef HAL_ADC_Init(ADC_HandleTypeDef *a)
{assert(a==&hadc4&&a->Init.LowPowerAutoWait==ENABLE);return HAL_OK;}
HAL_StatusTypeDef HAL_ADCEx_Calibration_Start(ADC_HandleTypeDef *a)
{assert(a==&hadc4);return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *a)
{assert(a==&hadc4&&biasOn);pollIndex=0U;return HAL_OK;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *a,uint32_t timeout)
{assert(a==&hadc4&&timeout==10U);++pollIndex;return pollIndex==timeoutRank?HAL_TIMEOUT:HAL_OK;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *a)
{assert(a==&hadc4);++adcReads;return pollIndex==1U?adcRaw:1234U;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *a)
{assert(a==&hadc4);++stopCalls;return HAL_OK;}
uint32_t HAL_LPTIM_ReadCounter(LPTIM_HandleTypeDef *t)
{assert(t==&hlptim1);return flowCounter;}
HAL_StatusTypeDef HAL_LPTIM_Counter_Start(LPTIM_HandleTypeDef *t)
{assert(t==&hlptim1&&boostOn&&flowPowerOn);return HAL_OK;}
void HAL_GPIO_WritePin(void *port,uint16_t pin,int state)
{
    (void)port;
    if(pin==NTC_BIAS_Pin)biasOn=state==GPIO_PIN_SET;
    else if(pin==BOOST_EN_Pin)boostOn=state==GPIO_PIN_SET;
    else if(pin==YFB7_EN_Pin)flowPowerOn=state==GPIO_PIN_SET;
    else assert(0);
}
void vTaskDelay(TickType_t ticks)
{
    if(ticks==pdMS_TO_TICKS(1000U)){
        if(loops++==stopAfter)longjmp(taskExit,1);
        flowCounter=(uint16_t)(flowCounter+11U);
    }
    Integration_TimeMs+=ticks*portTICK_PERIOD_MS;
}
void vTaskDelete(TaskHandle_t task){(void)task;longjmp(taskExit,2);}
static void RunSensor(unsigned int count)
{
    loops=0U;stopAfter=count;adcReads=0U;stopCalls=0U;flowCounter=65530U;
    int result=setjmp(taskExit);
    if(result==0)SensorTask(NULL);
    assert(result==1);assert(!biasOn);
}

int main(void)
{
    AppIpc_Init();AppIpc_Init();
    AppIpc_SetDeviceIeee(UINT64_C(0x00124B000ABCDEF0));
    assert(gStatus.latest_sample.valid==0U);
    assert(!AppIpc_PublishSample(NULL));
    gStatus.actuators.fan_duty_permille=400U;
    gStatus.applied_config.request_id=99U;
    gStatus.state=APP_STATE_IDLE;
    RunSensor(1U);
    AppStatus status;
    assert(AppIpc_ReadLocalStatus(&status));
    int16_t measured=status.latest_sample.temperature_centi_c;
    assert(measured>=2498&&measured<=2502);
    assert(status.latest_sample.valid==(APP_SAMPLE_VALID_TEMP|APP_SAMPLE_VALID_FLOW));
    assert(status.latest_sample.flow_ml_min==1000U); /* Includes counter rollover. */
    assert(status.latest_sample.battery_percent==APP_BATTERY_PERCENT_UNKNOWN);
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_BATTERY)==0U);
    assert(adcReads==2U&&stopCalls==1U);
    assert(status.latest_sample.timestamp_ms==HAL_GetTick());
    assert(status.latest_sample.source_node_id==APP_NODE_UNASSIGNED);
    assert(status.actuators.fan_duty_permille==400U&&status.applied_config.request_id==99U);
    assert(status.state==APP_STATE_IDLE);
    AppSample queued;
    assert(xQueueReceive(qSample,&queued,0U)==pdPASS);
    assert(queued.temperature_centi_c==measured&&queued.sequence==status.latest_sample.sequence);
    APP_ZIGBEE_UpdateTemperature();assert(writtenAttribute==measured);
    Integration_TimeMs+=5010U;
    APP_ZIGBEE_UpdateTemperature();assert(writtenAttribute==INT16_MIN);

    AppSample sample=queued;sample.timestamp_ms=HAL_GetTick();sample.source_node_id=99U;
    Integration_MutexAllowed=false;
    assert(!AppIpc_PublishSample(&sample));
    assert(xQueueReceive(qSample,&queued,0U)==pdFALSE);
    Integration_MutexAllowed=true;
    assert(AppIpc_PublishSample(&sample));
    assert(sample.source_node_id==99U);
    assert(xQueueReceive(qSample,&queued,0U)==pdPASS);
    assert(queued.source_node_id==APP_NODE_UNASSIGNED);
    APP_ZIGBEE_UpdateTemperature();assert(writtenAttribute==measured);
    sample.temperature_centi_c=50;sample.timestamp_ms=HAL_GetTick();
    assert(AppIpc_PublishSample(&sample));
    APP_ZIGBEE_UpdateTemperature();assert(writtenAttribute==INT16_MIN);

    adcRaw=0U;RunSensor(1U);assert(AppIpc_ReadLocalStatus(&status));
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)==0U);
    APP_ZIGBEE_UpdateTemperature();assert(writtenAttribute==INT16_MIN);
    adcRaw=4095U;RunSensor(1U);assert(AppIpc_ReadLocalStatus(&status));
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)==0U);
    adcRaw=2844U;timeoutRank=2U;RunSensor(1U);assert(AppIpc_ReadLocalStatus(&status));
    assert(adcReads==1U&&stopCalls==1U);
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)==0U);
    timeoutRank=1U;RunSensor(1U);assert(AppIpc_ReadLocalStatus(&status));
    assert(adcReads==0U&&stopCalls==1U);
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)==0U);
    timeoutRank=0U;RunSensor(2U);assert(AppIpc_ReadLocalStatus(&status));
    assert(status.latest_sample.sequence==2U);
    assert(xQueueReceive(qSample,&queued,0U)==pdPASS&&queued.sequence==2U);
    assert(xQueueReceive(qSample,&queued,0U)==pdFALSE);
    assert(attributeWrites>=5U);
    Integration_FreeObjects();
    printf("SENSOR_TEST_PASS temperature_centi_c=%d; original ADC + flow, real IPC snapshot, queue ownership, stale/range/invalid handling passed (fake HAL/RTOS).\n",measured);
    return 0;
}
