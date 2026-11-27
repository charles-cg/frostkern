#include "app_role.h"

#if APP_IS_MASTER

#include "ds18b20.h"
#include "main.h"

static bool conversionStarted;
static uint32_t convertedAt;
static DS18B20_Diagnostics busDiagnostic;
/* Board configuration already tested by Joaquin: TIM2 32 MHz -> 1 MHz.
 * Do not infer this timebase from a possibly stale SDK SystemCoreClock value.
 * If SystemClock_Config changes, verify DS-DIAG ticks/ms before using this driver. */
#define DS18B20_TIM2_PRESCALER 31U

void DS18B20_GetDiagnostics(DS18B20_Diagnostics *out)
{
    if(out==NULL) { return; }
    *out=busDiagnostic;
    out->counter=TIM2->CNT;
    out->pclk1_hz=HAL_RCC_GetPCLK1Freq(); /* diagnostic only, not the timebase */
    out->prescaler=(uint16_t)LL_TIM_GetPrescaler(TIM2);
    out->mode=(uint8_t)((GPIOB->MODER>>2U)&3U);
    out->open_drain=(uint8_t)((GPIOB->OTYPER>>1U)&1U);
}

// DQ es PB1 (SDA de CN6). TIM2 cuenta un tick por microsegundo.
__STATIC_FORCEINLINE void WaitFrom(uint32_t start, uint32_t us)
{
    while ((uint32_t)(TIM2->CNT - start) < us) {
    }
}

void DS18B20_Init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = GPIO_PIN_1;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &gpio);
    /* Release again AFTER mode configuration; OD high is high impedance. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);
    LL_TIM_DisableCounter(TIM2);
    LL_TIM_SetPrescaler(TIM2, DS18B20_TIM2_PRESCALER);
    LL_TIM_SetAutoReload(TIM2, UINT32_MAX);
    LL_TIM_GenerateEvent_UPDATE(TIM2);
    LL_TIM_ClearFlag_UPDATE(TIM2);
    LL_TIM_EnableCounter(TIM2);
}

static bool ResetBus(void)
{
    busDiagnostic.fault=0U;
    busDiagnostic.odr_before=(GPIOB->ODR & GPIO_PIN_1)!=0U;
    /* Explicit ownership at each transaction: never assume the previous output
     * latch is still released after startup or another peripheral's activity. */
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    WaitFrom(TIM2->CNT, 10U);
    busDiagnostic.odr_after=(GPIOB->ODR & GPIO_PIN_1)!=0U;
    busDiagnostic.input_high=0U;
    busDiagnostic.idle_high=(GPIOB->IDR & GPIO_PIN_1)!=0U;
    busDiagnostic.presence_low=0U;
    busDiagnostic.final_high=0U;
    if (!busDiagnostic.idle_high) {
        busDiagnostic.fault=1U;
        /* Probe with the MCU driver fully disconnected. No push-pull HIGH.
         * Preserve every other PB pin, including supply enables on PB5/PB7. */
        uint32_t irq=__get_PRIMASK(); __disable_irq();
        uint32_t mode=GPIOB->MODER & (3U<<2U);
        GPIOB->MODER &= ~(3U<<2U);
        __set_PRIMASK(irq);
        WaitFrom(TIM2->CNT, 20U);
        busDiagnostic.input_high=(GPIOB->IDR & GPIO_PIN_1)!=0U;
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
        irq=__get_PRIMASK(); __disable_irq();
        GPIOB->MODER=(GPIOB->MODER & ~(3U<<2U))|mode;
        __set_PRIMASK(irq);
        return false;
    }

    GPIOB->BSRR = (uint32_t)GPIO_PIN_1 << 16U;
    WaitFrom(TIM2->CNT, 500U);

    // Protege el instante de lectura del pulso de presencia.
    uint32_t irqState = __get_PRIMASK();
    __disable_irq();
    GPIOB->BSRR = GPIO_PIN_1;
    uint32_t releasedAt = TIM2->CNT;
    WaitFrom(releasedAt, 70U);
    bool present = (GPIOB->IDR & GPIO_PIN_1) == 0U;
    __set_PRIMASK(irqState);

    WaitFrom(releasedAt, 500U);
    busDiagnostic.presence_low=present;
    busDiagnostic.final_high=(GPIOB->IDR & GPIO_PIN_1)!=0U;
    busDiagnostic.fault=!present ? 2U : !busDiagnostic.final_high ? 3U : 0U;
    return present && busDiagnostic.final_high;
}

static void WriteByte(uint8_t value)
{
    for (uint8_t bit = 0U; bit < 8U; bit++) {
        uint32_t lowUs = 65U;
        if ((value & 1U) != 0U) {
            lowUs = 6U;
        }

        uint32_t irqState = __get_PRIMASK();
        __disable_irq();
        GPIOB->BSRR = (uint32_t)GPIO_PIN_1 << 16U;
        uint32_t start = TIM2->CNT;
        WaitFrom(start, lowUs);
        GPIOB->BSRR = GPIO_PIN_1;
        __set_PRIMASK(irqState);

        WaitFrom(start, 75U);
        value >>= 1U;
    }
}

static uint8_t ReadByte(void)
{
    uint8_t value = 0U;

    for (uint8_t bit = 0U; bit < 8U; bit++) {
        uint32_t irqState = __get_PRIMASK();
        __disable_irq();
        GPIOB->BSRR = (uint32_t)GPIO_PIN_1 << 16U;
        uint32_t start = TIM2->CNT;
        WaitFrom(start, 3U);
        GPIOB->BSRR = GPIO_PIN_1;
        WaitFrom(start, 12U);
        bool high = (GPIOB->IDR & GPIO_PIN_1) != 0U;
        __set_PRIMASK(irqState);

        if (high) {
            value |= (uint8_t)(1U << bit);
        }
        WaitFrom(start, 75U);
    }

    return value;
}

bool DS18B20_StartConversion(void)
{
    conversionStarted=false;
    if (!ResetBus()) {
        return false;
    }

    WriteByte(0xCCU); // Skip ROM: un sensor en DQ.
    WriteByte(0x44U); // Convert T.
    convertedAt=HAL_GetTick(); conversionStarted=true;
    return true;
}

DS18B20_Status DS18B20_ReadTemperature(int16_t *centiC)
{
    if(centiC==NULL || !conversionStarted || (uint32_t)(HAL_GetTick()-convertedAt)<750U) {
        return DS18B20_NOT_READY;
    }
    conversionStarted=false;
    if (!ResetBus()) {
        return DS18B20_NO_PRESENCE;
    }

    WriteByte(0xCCU); // Skip ROM.
    WriteByte(0xBEU); // Read Scratchpad.

    uint8_t data[9];
    for (uint8_t i = 0U; i < sizeof(data); i++) {
        data[i] = ReadByte();
    }

    return DS18B20_DecodeScratchpad(data,centiC);
}

#endif
