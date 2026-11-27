#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_ipc.h"
#include "fake_rtos.h"
#include "../Core/Src/app_master_control.c"
static uint8_t acOut,radOut;
void AppFanPwm_Init(void) {}
void AppFanPwm_Set(uint8_t a,uint8_t b){acOut=a;radOut=b;}
void DS18B20_Init(void) {}
void DS18B20_GetDiagnostics(DS18B20_Diagnostics *d){memset(d,0,sizeof(*d));}
bool DS18B20_StartConversion(void){return false;}
DS18B20_Status DS18B20_ReadTemperature(int16_t *t){(void)t;return DS18B20_NO_PRESENCE;}
void Error_Handler(void){assert(0);}
void vTaskDelete(TaskHandle_t task){(void)task;assert(0);}
int main(void){
 AppIpc_Init();AppMaster_Start();AppMaster_Start();Step(0U);assert(!acOut&&!radOut);
 uint8_t p[16]={1,0,0x80,0xE1,0xFF,0xFE,0x32,0xFC,0x18};
 /* 25 C OFF,30 C ON,40 C full,40% minimum are TEST data, not calibration. */
 p[9]=0xC4;p[10]=9;p[11]=0xB8;p[12]=11;p[13]=0xA0;p[14]=15;p[15]=40;
 assert(AppMaster_QueueCommand(APP_UART_SET_AUTO_FAN,p,15)==APP_UART_ERR_LENGTH);
 p[15]=0;assert(AppMaster_QueueCommand(APP_UART_SET_AUTO_FAN,p,16)==APP_UART_ERR_VALUE);p[15]=40;
 assert(AppMaster_QueueCommand(APP_UART_SET_AUTO_FAN,p,16)==0);Step(0U);assert(radOut==100U);
 /* Deliberately register Target2 FIRST. Control must still use FC18. */
 uint64_t t2=UINT64_C(0x0080E1FFFE32FC28),t1=APP_FAN_DEFAULT_TARGET_IEEE;
 assert(AppIpc_RegisterSlave(t2)==1U);assert(AppIpc_RegisterSlave(t1)==2U);
 AppIpc_MarkNetworkStarted();Integration_TimeMs=100U;
 assert(AppIpc_SaveSlaveTemperature(t2,9000,true));assert(AppIpc_SaveSlaveTemperature(t1,2400,true));
 Step(100U);assert(radOut==0U); /* Never use Target2's 90 C. */
 assert(AppIpc_SaveSlaveTemperature(t1,3000,true));Step(100U);assert(radOut==40U);
 assert(AppIpc_SaveSlaveTemperature(t1,3500,true));Step(100U);assert(radOut==70U);
 assert(AppIpc_SaveSlaveTemperature(t1,2700,true));Step(100U);assert(radOut==40U);
 assert(AppIpc_SaveSlaveTemperature(t1,2500,true));Step(100U);assert(radOut==0U);
 Step(90101U);assert(radOut==100U && state.radiator.reason==APP_FAN_NO_FRESH_INPUT);
 Integration_TimeMs=90200U;assert(AppIpc_SaveSlaveTemperature(t1,2600,true));
 assert(AppIpc_InvalidateSlaveMeasurements(t1));Step(90200U);assert(radOut==100U);
 uint8_t ac[2]={1,44};assert(AppMaster_QueueCommand(APP_UART_SET_AC_FAN,ac,2)==0);Step(90200U);assert(acOut==44U);
 ac[0]=0;assert(AppMaster_QueueCommand(APP_UART_SET_AC_FAN,ac,2)==0);Step(90200U);assert(acOut==0U&&state.ac_requested==44U);
 ac[1]=101;assert(AppMaster_QueueCommand(APP_UART_SET_AC_FAN,ac,2)==APP_UART_ERR_VALUE);
 uint8_t bench[2]={1,25};assert(AppMaster_QueueCommand(APP_UART_FAN_BENCH,bench,2)==0);Step(90200U);assert(radOut==25U&&!state.automatic.enabled);
 Step(95200U);assert(radOut==0U&&!state.bench_mode);
 for(unsigned i=0;i<8;i++)assert(AppMaster_QueueCommand(APP_UART_FAN_BENCH,bench,2)==0);
 assert(AppMaster_QueueCommand(APP_UART_FAN_BENCH,bench,2)==APP_UART_ERR_QUEUE_FULL);
 Step(UINT32_MAX-2U);assert(radOut==25U);Step(4997U);assert(radOut==0U);
 AppMasterSnapshot snap;assert(AppMaster_Read(&snap));
 Integration_FreeObjects();puts("PASS Master: IEEE Target1 despite reversed slots, hysteresis/ramp, new/rejoin/expired input fail-full, manual off, validation, queue-full, bench lease and rollover.");return 0;
}
