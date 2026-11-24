#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app_ipc.h"
#include "fake_rtos.h"
#include "main.h"
#include "log_module.h"

#define APP_ZIGBEE_ENDPOINT 17U
#define ZCL_TEMP_MEAS_ATTR_MEAS_VAL 0U
#define ZCL_TEMP_MEAS_UNKNOWN 0x8000U
#define ZB_APSDE_ADDRMODE_EXT 3
#define ZB_APSDE_ADDRMODE_SHORT 2
enum ZclDataTypeT {ZCL_DATATYPE_SIGNED_16BIT=0x29};
enum ZclStatusCodeT {ZCL_STATUS_SUCCESS=0,ZCL_STATUS_FAILURE=1};
struct ZbZclClusterT {unsigned int unused;};
struct ZbZclHeaderT {unsigned int unused;};
struct ZbApsdeDataIndT {
    struct {uint16_t endpoint;} dst;
    struct {uint16_t endpoint;int mode;uint16_t nwkAddr;uint64_t extAddr;} src;
};
static int zigbee;
static struct {void *pstZigbee;} stZigbeeAppInfo={&zigbee};
static long long ZbZclParseInteger(enum ZclDataTypeT type,const uint8_t *p,
                                   enum ZclStatusCodeT *status)
{
    assert(type==ZCL_DATATYPE_SIGNED_16BIT);
    *status=ZCL_STATUS_SUCCESS;
    return (int16_t)((uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8U));
}
static uint64_t ZbNwkAddrLookupExt(void *zb,uint16_t address)
{
    assert(zb==&zigbee);
    return address==0x1234U?UINT64_C(0x00124B000ABCDEF0):
           address==0x5678U?UINT64_C(0x00124B000ABCDEF1):0U;
}

/* These snippets are extracted from the production files by the test runner. */
#include "zigbee_report_callback.inc"
#include "zigbee_master_dispatch.inc"
#include "../../Core/Src/app_uart_bridge.c"

UART_HandleTypeDef huart2;
static AppUartFrame lastFrame;
static unsigned int frameCount;
static FILE *capture;
static unsigned int Drain(void)
{
    unsigned int count=0U;AppZbCommand command;
    while(xQueueReceive(qZbCommand,&command,0U)==pdPASS){
        assert(command.type==APP_ZB_COMMAND_TEMP_RECEIVED);
        Integration_DispatchTemperature(command);++count;
    }
    return count;
}
static void Report(struct ZbApsdeDataIndT *data,int16_t value)
{
    uint8_t payload[2]={(uint8_t)value,(uint8_t)((uint16_t)value>>8U)};
    APP_ZIGBEE_TemperatureReport(NULL,NULL,data,0U,ZCL_DATATYPE_SIGNED_16BIT,
                                payload,2U,NULL);
}
static uint32_t Read32(const uint8_t *p)
{return (uint32_t)p[0]|(uint32_t)p[1]<<8U|(uint32_t)p[2]<<16U|(uint32_t)p[3]<<24U;}
void Error_Handler(void){assert(0);}
void MX_USART2_UART_Init(void){}
uint32_t HAL_GetTick(void){return Integration_TimeMs;}
void vTaskDelete(TaskHandle_t task){(void)task;assert(0);}
void HAL_NVIC_DisableIRQ(int irq){assert(irq==USART2_IRQn);}
void HAL_NVIC_EnableIRQ(int irq){assert(irq==USART2_IRQn);}
void HAL_NVIC_SetPriority(int irq,uint32_t priority,uint32_t sub)
{assert(irq==USART2_IRQn&&priority==8U&&sub==0U);}
void HAL_GPIO_WritePin(void *port,uint16_t pin,int state)
{(void)port;(void)pin;(void)state;}
HAL_StatusTypeDef HAL_UART_RegisterCallback(UART_HandleTypeDef *u,
    HAL_UART_CallbackIDTypeDef id,void (*fn)(UART_HandleTypeDef *))
{assert(u==&huart2&&fn!=NULL);(void)id;return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{assert(u==&huart2&&p!=NULL&&n==1U);return HAL_OK;}
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u,uint8_t *p,uint16_t n)
{
    assert(u==&huart2);AppUartParser decoder;AppUartParser_Init(&decoder);
    for(uint16_t i=0;i<n;++i)AppUartParser_Push(&decoder,p[i]);
    assert(AppUartParser_Next(&decoder,&lastFrame));++frameCount;
    if(capture!=NULL)assert(fwrite(p,1U,n,capture)==n);
    TxComplete(u);return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u)
{assert(u==&huart2);return HAL_OK;}
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u)
{assert(u==&huart2);return HAL_OK;}

int main(int argc,char **argv)
{
    assert(argc==3);
    int16_t measured=(int16_t)strtol(argv[1],NULL,10);
    assert(measured>=2498&&measured<=2502);
    capture=fopen(argv[2],"wb");assert(capture!=NULL);
    AppIpc_Init();AppUartBridge_Start();Integration_TimeMs=10000U;
    struct ZbApsdeDataIndT source={.dst={17U},
        .src={17U,ZB_APSDE_ADDRMODE_EXT,0U,UINT64_C(0x00124B000ABCDEF0)}};
    Report(&source,measured);assert(Drain()==1U);
    AppStatus status;
    assert(AppIpc_ReadSlaveStatus(1U,&status));
    assert(status.latest_sample.temperature_centi_c==measured);
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)!=0U);
    assert(status.device.ieee_address[0]==0U&&status.device.ieee_address[1]==0x12U);
    assert(status.device.ieee_address[7]==0xF0U);
    PublishSlave(1U);
    assert(frameCount==2U&&lastFrame.type==APP_UART_TELEMETRY_TEMP);
    assert(lastFrame.payload[1]==1U&&lastFrame.payload[10]==5U);
    assert((int16_t)((uint16_t)lastFrame.payload[11]|
                    (uint16_t)((uint16_t)lastFrame.payload[12]<<8U))==measured);
    assert(Read32(&lastFrame.payload[13])==10000U&&Read32(&lastFrame.payload[17])==0U);

    source.src.mode=ZB_APSDE_ADDRMODE_SHORT;source.src.extAddr=0U;
    source.src.nwkAddr=0x5678U;Integration_TimeMs+=10U;
    Report(&source,(int16_t)(measured+100));assert(Drain()==1U);
    assert(AppIpc_ReadSlaveStatus(2U,&status));
    assert(status.latest_sample.temperature_centi_c==measured+100);
    PublishSlave(2U);assert(lastFrame.payload[1]==2U&&lastFrame.payload[9]==0xF1U);
    assert(AppIpc_ReadSlaveStatus(1U,&status));
    assert(status.latest_sample.temperature_centi_c==measured);

    /* Application callback rejects wrong endpoint/type/attribute/length. */
    uint8_t bytes[2]={1U,2U};
    source.dst.endpoint=18U;Report(&source,measured);assert(Drain()==0U);
    source.dst.endpoint=17U;source.src.endpoint=18U;
    Report(&source,measured);assert(Drain()==0U);source.src.endpoint=17U;
    APP_ZIGBEE_TemperatureReport(NULL,NULL,&source,1U,ZCL_DATATYPE_SIGNED_16BIT,bytes,2U,NULL);
    APP_ZIGBEE_TemperatureReport(NULL,NULL,&source,0U,(enum ZclDataTypeT)0x21,bytes,2U,NULL);
    APP_ZIGBEE_TemperatureReport(NULL,NULL,&source,0U,ZCL_DATATYPE_SIGNED_16BIT,bytes,1U,NULL);
    APP_ZIGBEE_TemperatureReport(NULL,NULL,&source,0U,ZCL_DATATYPE_SIGNED_16BIT,NULL,2U,NULL);
    APP_ZIGBEE_TemperatureReport(NULL,NULL,NULL,0U,ZCL_DATATYPE_SIGNED_16BIT,bytes,2U,NULL);
    assert(Drain()==0U);

    source.src.nwkAddr=0x7777U;Report(&source,measured);assert(Drain()==1U);
    assert(AppIpc_ReadSlaveStatus(2U,&status));
    assert(status.latest_sample.temperature_centi_c==measured+100);
    source.src.nwkAddr=0x1234U;Integration_TimeMs+=10U;
    Report(&source,INT16_MIN);assert(Drain()==1U);
    assert(AppIpc_ReadSlaveStatus(1U,&status));
    assert((status.latest_sample.valid&APP_SAMPLE_VALID_TEMP)==0U);
    PublishSlave(1U);assert(lastFrame.payload[10]==APP_UART_TEMP_REPORT_SEEN);
    assert(lastFrame.payload[11]==0U&&lastFrame.payload[12]==0U);

    Report(&source,measured);assert(Drain()==1U);
    Integration_TimeMs+=180010U;
    PublishSlave(1U);
    assert(lastFrame.payload[10]==(APP_UART_TEMP_STALE|APP_UART_TEMP_REPORT_SEEN));
    assert(Read32(&lastFrame.payload[17])==180010U);
    fclose(capture);capture=NULL;
    Integration_FreeObjects();
    puts("MASTER_TEST_PASS: production report callback + production dispatch + real IPC + UART frames; IEEE separation, malformed data, unknown source/value and stale handling passed (fake SDK/RTOS).");
    return 0;
}
