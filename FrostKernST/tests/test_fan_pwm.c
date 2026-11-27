#include <stdio.h>
#include "../Core/Src/app_fan_pwm.c"
FakeTimer timer3;
static unsigned gpioCalls;
void HAL_GPIO_WritePin(void *p,uint32_t pins,int state){assert(p==GPIOA&&pins==(GPIO_PIN_1|GPIO_PIN_10)&&state==0);}
void HAL_GPIO_Init(void *p,GPIO_InitTypeDef *g){assert(p==GPIOA&&g->Pin==(GPIO_PIN_1|GPIO_PIN_10));if(gpioCalls++==1)assert(g->Mode==GPIO_MODE_AF_PP&&g->Alternate==2);}
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef *c,uint32_t *l){c->APB1CLKDivider=2;*l=0;}
uint32_t HAL_RCC_GetPCLK1Freq(void){return 32000000U;}
void Error_Handler(void){assert(0);}
int main(void){AppFanPwm_Init();assert(timer3.psc==63&&timer3.arr==999&&timer3.c1==0&&timer3.c2==0&&timer3.enabled);
 AppFanPwm_Set(100,0);assert(timer3.c2==timer3.arr+1&&timer3.c1==0);
 AppFanPwm_Set(11,73);assert(timer3.c2==110&&timer3.c1==730);
 AppFanPwm_Set(0,100);assert(timer3.c2==0&&timer3.c1==1000);
 AppFanPwm_Set(255,255);assert(timer3.c1==1000&&timer3.c2==1000);
 puts("PASS real PWM driver with fake registers: PA1/CH2 vs PA10/CH1 independence, APB timer clock, true 0/100 percent and clamping.");}
