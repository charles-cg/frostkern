#ifndef DS_FAKE_MAIN_H
#define DS_FAKE_MAIN_H
#include <stdint.h>
#include <stddef.h>
#define __STATIC_FORCEINLINE static inline
#define GPIO_PIN_1 2U
#define GPIO_PIN_SET 1
#define GPIO_MODE_OUTPUT_OD 17
#define GPIO_NOPULL 0
#define GPIO_SPEED_FREQ_LOW 0
#define LL_APB1_GRP1_PERIPH_TIM2 1
#define __HAL_RCC_GPIOB_CLK_ENABLE() ((void)0)
typedef struct{uint32_t CNT,PSC,ARR,enabled;}TestTimer;
typedef struct{uint32_t IDR,BSRR,MODER,OTYPER,ODR;}TestGpio;
typedef struct{uint32_t Pin,Mode,Pull,Speed;}GPIO_InitTypeDef;
TestTimer *Test_GetTimer(void);
TestGpio *Test_GetGpio(void);
#define TIM2 Test_GetTimer()
#define GPIOB Test_GetGpio()
#define LL_APB1_GRP1_EnableClock(x) ((void)(x))
#define LL_TIM_DisableCounter(t) ((t)->enabled=0U)
#define LL_TIM_EnableCounter(t) ((t)->enabled=1U)
#define LL_TIM_SetPrescaler(t,v) ((t)->PSC=(v))
#define LL_TIM_GetPrescaler(t) ((t)->PSC)
#define LL_TIM_SetAutoReload(t,v) ((t)->ARR=(v))
#define LL_TIM_GenerateEvent_UPDATE(t) ((t)->CNT=0U)
#define LL_TIM_ClearFlag_UPDATE(t) ((void)(t))
uint32_t HAL_GetTick(void);
uint32_t HAL_RCC_GetPCLK1Freq(void);
void HAL_GPIO_WritePin(TestGpio*,uint32_t,int);
void HAL_GPIO_Init(TestGpio*,GPIO_InitTypeDef*);
static inline uint32_t __get_PRIMASK(void){return 0U;}
static inline void __disable_irq(void){}
static inline void __set_PRIMASK(uint32_t m){(void)m;}
#endif
