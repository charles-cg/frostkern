#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include "../Core/Src/ds18b20.c"
static TestTimer timer;
static TestGpio gpio;
static uint32_t appliedBsrr,lowSince,releasedAt,lastLow;
static bool hostLow,respond=true,idleLow,stayLow,haveReset;
static void ApplyBus(void){
 if(gpio.BSRR!=appliedBsrr){
  appliedBsrr=gpio.BSRR;
  if(appliedBsrr==(GPIO_PIN_1<<16U)){hostLow=true;gpio.ODR&=~GPIO_PIN_1;lowSince=timer.CNT;}
  if(appliedBsrr==GPIO_PIN_1){
   if(hostLow){lastLow=timer.CNT-lowSince;haveReset=lastLow>=480U;}
   hostLow=false;gpio.ODR|=GPIO_PIN_1;releasedAt=timer.CNT;
  }
 }
 uint32_t since=timer.CNT-releasedAt;
 bool presence=haveReset&&respond&&since>=20U&&(since<200U||stayLow);
 bool output=((gpio.MODER>>2U)&3U)==1U;
 gpio.IDR=(hostLow&&output)||presence||idleLow?0U:GPIO_PIN_1;
}
TestTimer *Test_GetTimer(void){if(timer.enabled)++timer.CNT;ApplyBus();return &timer;}
TestGpio *Test_GetGpio(void){ApplyBus();return &gpio;}
uint32_t HAL_GetTick(void){return timer.CNT/1000U;}
uint32_t HAL_RCC_GetPCLK1Freq(void){return 16000000U;} /* Must not replace tested PSC with 15. */
void HAL_GPIO_WritePin(TestGpio *g,uint32_t pin,int s){assert(g==&gpio&&pin==2&&s==1);g->BSRR=2;}
void HAL_GPIO_Init(TestGpio *g,GPIO_InitTypeDef *c){assert(g==&gpio&&c->Pin==2&&c->Mode==GPIO_MODE_OUTPUT_OD);g->MODER=1U<<2;g->OTYPER=2;}
int main(void){
 DS18B20_Init();assert(timer.PSC==31U&&timer.ARR==UINT32_MAX);
 /* Startup with a stale LOW latch must be released before checking idle. */
 gpio.BSRR=GPIO_PIN_1<<16U;ApplyBus();assert(gpio.ODR==0U);
 assert(ResetBus());assert(lastLow>=500U&&lastLow<510U);
 DS18B20_Diagnostics d;DS18B20_GetDiagnostics(&d);
 assert(d.fault==0&&d.idle_high&&d.presence_low&&d.final_high&&d.mode==1&&d.open_drain==1);
 assert(d.prescaler==31U&&d.pclk1_hz==16000000U);
 assert(d.odr_before==0U&&d.odr_after==1U);
 respond=false;assert(!ResetBus());DS18B20_GetDiagnostics(&d);assert(d.fault==2&&d.idle_high&&!d.presence_low&&d.final_high);
 gpio.MODER|=(1U<<10U)|(1U<<14U);gpio.ODR|=(1U<<5U)|(1U<<7U);
 uint32_t otherModes=gpio.MODER&~(3U<<2U);
 idleLow=true;assert(!ResetBus());DS18B20_GetDiagnostics(&d);assert(d.fault==1&&!d.idle_high&&!d.input_high&&d.odr_after);
 assert(((gpio.MODER>>2U)&3U)==1U&&gpio.OTYPER==2U);
 assert((gpio.MODER&~(3U<<2U))==otherModes);
 assert((gpio.ODR&((1U<<5U)|(1U<<7U)))==((1U<<5U)|(1U<<7U)));
 idleLow=false;respond=true;stayLow=true;haveReset=false;assert(!ResetBus());DS18B20_GetDiagnostics(&d);assert(d.fault==3&&d.presence_low&&!d.final_high);
 stayLow=false;haveReset=false;timer.CNT=UINT32_MAX-250U;assert(ResetBus());
 puts("PASS real DS18B20 bus code with fake GPIO/TIM: stale LOW latch recovery, tri-state probe, mode restore, reset/presence and faults.");
}
