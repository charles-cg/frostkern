#include "app_role.h"
#if APP_IS_MASTER
#include "main.h"
#include "app_fan_policy.h"
#include "app_fan_pwm.h"
static uint32_t period;
void AppFanPwm_Init(void)
{
    /* One timer, TWO compare registers. TIM2 remains exclusive to 1-Wire. */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    __HAL_RCC_GPIOA_CLK_ENABLE();
    /* Hold both enables LOW before changing them to alternate function. */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_10, GPIO_PIN_RESET);
    GPIO_InitTypeDef gpio={0};
    gpio.Pin=GPIO_PIN_1|GPIO_PIN_10; gpio.Mode=GPIO_MODE_OUTPUT_PP;
    gpio.Pull=GPIO_PULLDOWN; gpio.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA,&gpio);
    RCC_ClkInitTypeDef clock={0}; uint32_t latency;
    HAL_RCC_GetClockConfig(&clock,&latency);
    uint32_t hz=HAL_RCC_GetPCLK1Freq();
    if (clock.APB1CLKDivider != RCC_HCLK_DIV1) { hz*=2U; }
    uint32_t prescaler=(hz+999999U)/1000000U;
    if (prescaler==0U) { prescaler=1U; }
    period=hz/prescaler/APP_FAN_PWM_HZ;
    if (period < 2U || period > 65535U) { Error_Handler(); return; }
    LL_TIM_DisableCounter(TIM3);
    LL_TIM_SetPrescaler(TIM3,prescaler-1U);
    LL_TIM_SetAutoReload(TIM3,period-1U);
    LL_TIM_SetCounterMode(TIM3,LL_TIM_COUNTERMODE_UP);
    LL_TIM_OC_SetMode(TIM3,LL_TIM_CHANNEL_CH1,LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_SetMode(TIM3,LL_TIM_CHANNEL_CH2,LL_TIM_OCMODE_PWM1);
    LL_TIM_OC_SetPolarity(TIM3,LL_TIM_CHANNEL_CH1,LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_OC_SetPolarity(TIM3,LL_TIM_CHANNEL_CH2,LL_TIM_OCPOLARITY_HIGH);
    LL_TIM_OC_SetCompareCH1(TIM3,0U); LL_TIM_OC_SetCompareCH2(TIM3,0U);
    LL_TIM_OC_EnablePreload(TIM3,LL_TIM_CHANNEL_CH1);
    LL_TIM_OC_EnablePreload(TIM3,LL_TIM_CHANNEL_CH2);
    LL_TIM_GenerateEvent_UPDATE(TIM3); LL_TIM_ClearFlag_UPDATE(TIM3);
    LL_TIM_CC_EnableChannel(TIM3,LL_TIM_CHANNEL_CH1|LL_TIM_CHANNEL_CH2);
    gpio.Mode=GPIO_MODE_AF_PP; gpio.Alternate=GPIO_AF2_TIM3;
    HAL_GPIO_Init(GPIOA,&gpio);
    LL_TIM_EnableCounter(TIM3);
}
void AppFanPwm_Set(uint8_t ac_percent, uint8_t radiator_percent)
{
    if(ac_percent>100U) { ac_percent=100U; }
    if(radiator_percent>100U) { radiator_percent=100U; }
    /* Compare == ARR+1 gives a true continuous HIGH at 100%, 0 gives LOW. */
    LL_TIM_OC_SetCompareCH2(TIM3,(period*ac_percent+50U)/100U); /* PA1 ENA */
    LL_TIM_OC_SetCompareCH1(TIM3,(period*radiator_percent+50U)/100U); /* PA10 ENB */
}
#endif
