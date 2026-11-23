#include "sensor_task.h"
#include "app_role.h"

#if APP_IS_SLAVE

#include <math.h>
#include "main.h"
#include "app_task_config.h"
#include "task.h"
#include "log_module.h"

#define NTC_R_FIXED_OHM  0U       /* 0: mostrar solo el ADC crudo. */
#define NTC_R25_OHM      50000.0f
#define NTC_BETA_K       3950.0f
#define ADC_MAX_RAW      4095U

extern ADC_HandleTypeDef hadc4;

static HAL_StatusTypeDef ReadNtcRaw(uint32_t *raw)
{
    HAL_StatusTypeDef status = HAL_ADC_Start(&hadc4);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_ADC_PollForConversion(&hadc4, 10U);
    if (status == HAL_OK) {
        *raw = HAL_ADC_GetValue(&hadc4);       /* PA5: NTC */
        status = HAL_ADC_PollForConversion(&hadc4, 10U);
        if (status == HAL_OK) {
            (void)HAL_ADC_GetValue(&hadc4);    /* PA6: batería */
        }
    }

    HAL_StatusTypeDef stopStatus = HAL_ADC_Stop(&hadc4);
    return (status == HAL_OK) ? stopStatus : status;
}

static void SensorTask(void *argument)
{
    (void)argument;

    MX_ADC4_Init();
    hadc4.Init.LowPowerAutoWait = ENABLE;
    if (HAL_ADC_Init(&hadc4) != HAL_OK ||
        HAL_ADCEx_Calibration_Start(&hadc4) != HAL_OK) {
        LOG_ERROR_APP("NTC: error al inicializar o calibrar ADC4");
        vTaskDelete(NULL);
        return;
    }

    for (;;) {
        uint32_t raw = 0U;

        HAL_GPIO_WritePin(NTC_BIAS_GPIO_Port, NTC_BIAS_Pin, GPIO_PIN_SET);
        vTaskDelay(pdMS_TO_TICKS(20U));
        HAL_StatusTypeDef status = ReadNtcRaw(&raw);
        HAL_GPIO_WritePin(NTC_BIAS_GPIO_Port, NTC_BIAS_Pin, GPIO_PIN_RESET);

        if (status != HAL_OK) {
            LOG_ERROR_APP("NTC: error de lectura ADC4 (%d)", (int)status);
        } else if (raw == 0U || raw >= ADC_MAX_RAW) {
            LOG_WARNING_APP("NTC: ADC en un extremo, raw=%lu", (unsigned long)raw);
        } else if (NTC_R_FIXED_OHM == 0U) {
            LOG_INFO_APP("NTC: raw=%lu", (unsigned long)raw);
        } else {
            float resistance = (float)NTC_R_FIXED_OHM * (float)raw /
                               (float)(ADC_MAX_RAW - raw);
            float inverseKelvin = 1.0f / 298.15f +
                                  logf(resistance / NTC_R25_OHM) / NTC_BETA_K;
            long centiC = lroundf((1.0f / inverseKelvin - 273.15f) * 100.0f);
            LOG_INFO_APP("NTC: raw=%lu, T=%ld cC", (unsigned long)raw, centiC);
        }

        vTaskDelay(pdMS_TO_TICKS(1000U));
    }
}

void SensorTask_Start(void)
{
    if (xTaskCreate(SensorTask, "SensorTask",
                    APP_STACK_DEPTH(APP_TASK_STACK_SENSOR),
                    NULL, APP_TASK_PRIO_SENSOR, NULL) != pdPASS) {
        Error_Handler();
    }
}

#endif
