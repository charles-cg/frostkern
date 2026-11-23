#include "sensor_task.h"
#include "app_role.h"

#if APP_IS_SLAVE

#include <math.h>
#include "main.h"
#include "app_task_config.h"
#include "task.h"
#include "log_module.h"
#include "app_ipc.h"

#define NTC_R_FIXED_OHM  22000U       /* 0: mostrar solo el ADC crudo. */
#define NTC_R25_OHM      50000.0f
#define NTC_BETA_K       3950.0f
#define ADC_MAX_RAW      4095U
#define FLOW_HZ_PER_L_MIN  11U

extern ADC_HandleTypeDef hadc4;

extern LPTIM_HandleTypeDef hlptim1;

static uint16_t ReadFlowCounter(void)
{
    uint32_t first;
    uint32_t second;

    do {
        first = HAL_LPTIM_ReadCounter(&hlptim1);
        second = HAL_LPTIM_ReadCounter(&hlptim1);
    } while (first != second);

    return (uint16_t)second;
}

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

    MX_LPTIM1_Init();

    HAL_GPIO_WritePin(BOOST_EN_GPIO_Port, BOOST_EN_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(50U));
    HAL_GPIO_WritePin(YFB7_EN_GPIO_Port, YFB7_EN_Pin, GPIO_PIN_SET);
    vTaskDelay(pdMS_TO_TICKS(100U));

    bool flowReady = HAL_LPTIM_Counter_Start(&hlptim1) == HAL_OK;
    if (!flowReady) {
        LOG_ERROR_APP("FLOW: error al arrancar LPTIM1\r\n");
        HAL_GPIO_WritePin(YFB7_EN_GPIO_Port, YFB7_EN_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BOOST_EN_GPIO_Port, BOOST_EN_Pin, GPIO_PIN_RESET);
    }

    uint16_t previousCount = flowReady ? ReadFlowCounter() : 0U;
    TickType_t previousTick = xTaskGetTickCount();
    uint32_t sequence = 0U;

    for (;;) {

        vTaskDelay(pdMS_TO_TICKS(1000U));

        AppSample sample = {
            .source_node_id = gDeviceId.node_id,
            .sequence = ++sequence,
            .battery_percent = APP_BATTERY_PERCENT_UNKNOWN
        };

        if (flowReady) {
            uint16_t count = ReadFlowCounter();
            TickType_t now = xTaskGetTickCount();

            uint16_t pulses = (uint16_t)(count - previousCount);
            TickType_t elapsedTicks = now - previousTick;
            previousCount = count;
            previousTick = now;

            if (elapsedTicks != 0U) {
                uint64_t numerator = (uint64_t)pulses * 1000U * configTICK_RATE_HZ;
                uint64_t denominator = (uint64_t)FLOW_HZ_PER_L_MIN * elapsedTicks;

                sample.flow_ml_min =
                    (uint32_t)((numerator + denominator / 2U) / denominator);
                sample.valid |= APP_SAMPLE_VALID_FLOW;

                LOG_INFO_APP("FLOW: pulsos=%u, Q=%lu mL/min\r\n",
                             (unsigned int)pulses, (unsigned long)sample.flow_ml_min);
            }
        }

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
            if (centiC >= INT16_MIN && centiC <= INT16_MAX) {
                sample.temperature_centi_c = (int16_t)centiC;
                sample.valid |= APP_SAMPLE_VALID_TEMP;
            }
            LOG_INFO_APP("NTC: raw=%lu, T=%ld cC", (unsigned long)raw, centiC);
        }

        sample.timestamp_ms = HAL_GetTick();
        if (!AppIpc_PublishSample(&sample)) {
            LOG_WARNING_APP("SensorTask: no se pudo publicar la muestra\r\n");
        }

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
