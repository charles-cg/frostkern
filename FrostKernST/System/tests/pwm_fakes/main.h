#ifndef PWM_FAKE_MAIN_H
#define PWM_FAKE_MAIN_H
#include <assert.h>
#include <stdint.h>
#define GPIO_PIN_1 2U
#define GPIO_PIN_10 1024U
#define GPIO_PIN_RESET 0
#define GPIO_MODE_OUTPUT_PP 1
#define GPIO_MODE_AF_PP 2
#define GPIO_PULLDOWN 1
#define GPIO_SPEED_FREQ_LOW 0
#define GPIO_AF2_TIM3 2
#define GPIOA ((void*)1)
#define RCC_HCLK_DIV1 1
#define LL_APB1_GRP1_PERIPH_TIM3 1
#define LL_TIM_COUNTERMODE_UP 0
#define LL_TIM_CHANNEL_CH1 1
#define LL_TIM_CHANNEL_CH2 2
#define LL_TIM_OCMODE_PWM1 6
#define LL_TIM_OCPOLARITY_HIGH 0
#define __HAL_RCC_GPIOA_CLK_ENABLE() ((void)0)
typedef struct{uint32_t Pin,Mode,Pull,Speed,Alternate;}GPIO_InitTypeDef;
typedef struct{uint32_t APB1CLKDivider;}RCC_ClkInitTypeDef;
typedef struct{uint32_t arr,psc,c1,c2,enabled;}FakeTimer;
extern FakeTimer timer3;
#define TIM3 (&timer3)
void HAL_GPIO_WritePin(void*,uint32_t,int);
void HAL_GPIO_Init(void*,GPIO_InitTypeDef*);
void HAL_RCC_GetClockConfig(RCC_ClkInitTypeDef*,uint32_t*);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void Error_Handler(void);
#define LL_APB1_GRP1_EnableClock(x) ((void)(x))
#define LL_TIM_DisableCounter(t) ((t)->enabled=0)
#define LL_TIM_EnableCounter(t) ((t)->enabled=1)
#define LL_TIM_SetPrescaler(t,v) ((t)->psc=(v))
#define LL_TIM_SetAutoReload(t,v) ((t)->arr=(v))
#define LL_TIM_SetCounterMode(t,v) ((void)(t),(void)(v))
#define LL_TIM_OC_SetMode(t,c,v) ((void)(t),(void)(c),(void)(v))
#define LL_TIM_OC_SetPolarity(t,c,v) ((void)(t),(void)(c),(void)(v))
#define LL_TIM_OC_SetCompareCH1(t,v) ((t)->c1=(v))
#define LL_TIM_OC_SetCompareCH2(t,v) ((t)->c2=(v))
#define LL_TIM_OC_EnablePreload(t,c) ((void)(t),(void)(c))
#define LL_TIM_GenerateEvent_UPDATE(t) ((void)(t))
#define LL_TIM_ClearFlag_UPDATE(t) ((void)(t))
#define LL_TIM_CC_EnableChannel(t,c) ((void)(t),(void)(c))
#endif
