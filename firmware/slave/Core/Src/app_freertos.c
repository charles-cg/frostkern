/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : FreeRTOS applicative file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_common.h"
#include "fk_config.h"
#include "fk_rtos.h"
#include "fk_types.h"
#include "log_module.h"
#include "main.h"
#include "portmacrocommon.h"
#include "projdefs.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
static const char *volatile fk_overflowTask;
EventGroupHandle_t fkEventHandle;
TaskHandle_t linkTaskHandle;
volatile enum ZbStatusCodeT fkRejoinStatus;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 128 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
static void SupervisorTask(void *argument);
static void LinkTask(void *argument);
static void fkRejoinCallback(struct ZbNlmeJoinConfT *conf, void *arg);
static bool fkTryRejoinOnce(void);
/* USER CODE END FunctionPrototypes */

/* USER CODE BEGIN 4 */
void vApplicationStackOverflowHook(xTaskHandle xTask, char *pcTaskName) {
  /* Run time stack overflow checking is performed if
  configCHECK_FOR_STACK_OVERFLOW is defined to 1 or 2. This hook function is
  called if a stack overflow is detected. */
  fk_overflowTask = pcTaskName;
  Error_Handler();
}
/* USER CODE END 4 */

/* USER CODE BEGIN VPORT_SUPPORT_TICKS_AND_SLEEP */
__weak void vPortSuppressTicksAndSleep(TickType_t xExpectedIdleTime) {
  // Generated when configUSE_TICKLESS_IDLE == 2.
  // Function called in tasks.c (in portTASK_FUNCTION).
  // TO BE COMPLETED or TO BE REPLACED by a user one, overriding that weak one.
}
/* USER CODE END VPORT_SUPPORT_TICKS_AND_SLEEP */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */
  /* creation of defaultTask */
  defaultTaskHandle =
      osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  if (xTaskCreate(SupervisorTask, "SupervisorTask",
                  FK_SUPERVISOR_TASK_STACK_BYTES / sizeof(StackType_t), NULL,
                  FK_SUPERVISOR_TASK_PRIO, NULL) != pdPASS) {
    Error_Handler();
  }

  if (xTaskCreate(LinkTask, "LinkTask",
                  FK_LINK_TASK_STACK_BYTES / sizeof(StackType_t), NULL,
                  FK_LINK_TASK_PRIO, &linkTaskHandle) != pdPASS) {
    Error_Handler();
  }
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  fkEventHandle = xEventGroupCreate();
  if (fkEventHandle == NULL) {
    Error_Handler();
  }
  /* USER CODE END RTOS_EVENTS */
}
/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief Function implementing the defaultTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument) {
  /* USER CODE BEGIN defaultTask */
  /* Infinite loop */
  for (;;) {
    osDelay(1);
  }
  /* USER CODE END defaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
static void SupervisorTask(void *argument) {
  (void)argument;

  TickType_t lastLog = xTaskGetTickCount();

  for (;;) {
    HAL_GPIO_TogglePin(LD1_GPIO_Port, LD1_Pin);
    vTaskDelay(pdMS_TO_TICKS(FK_LED_BLINK_MS));
    TickType_t now = xTaskGetTickCount();
    if (now - lastLog >= pdMS_TO_TICKS(FK_DIAG_PERIOD_MS)) {
      UBaseType_t supFreeWords  = uxTaskGetStackHighWaterMark(NULL);
      UBaseType_t linkFreeWords = uxTaskGetStackHighWaterMark(linkTaskHandle);
      size_t      heapMinFree   = xPortGetMinimumEverFreeHeapSize();

      LOG_INFO_APP("Diag: stack free sup=%u B link=%u B, heap min free=%u B",
                   (unsigned)(supFreeWords * sizeof(StackType_t)),
                   (unsigned)(linkFreeWords * sizeof(StackType_t)),
                   (unsigned)heapMinFree);
      lastLog = now;
    }
  }
}

static void LinkTask(void *argument) {
  (void)argument;

  for (;;) {
    EventBits_t bits = xEventGroupWaitBits(fkEventHandle, FK_EV_PARENT_LOST,
                                           pdTRUE, pdFALSE, portMAX_DELAY);
    if ((bits & FK_EV_PARENT_LOST) == 0U) {
      continue;
    }
    LOG_INFO_APP("Link: parent lost, trying to rejoin");

    /* Retry with exponential backoff (piece 3). After FK_REJOIN_MAX_ATTEMPTS
       failures, drop the old network and let ST's task do a full join
       (piece 4). */
    uint32_t backoffMs = FK_REJOIN_BACKOFF_MIN_MS;
    uint32_t attempt = 0U;

    while (!fkTryRejoinOnce()) {
      attempt++;
      if (attempt >= FK_REJOIN_MAX_ATTEMPTS) {
        LOG_ERROR_APP("Link: %u rejoins failed, falling back to a full join",
                      (unsigned)attempt);
        ZbReset(stZigbeeAppInfo.pstZigbee);
        osSemaphoreRelease(NwkFormStartSemaphore);
        break;
      }
      LOG_INFO_APP("Link: attempt %u failed, retrying in %u ms",
                   (unsigned)attempt, (unsigned)backoffMs);
      vTaskDelay(pdMS_TO_TICKS(backoffMs));

      backoffMs *= 2U;
      if (backoffMs > FK_REJOIN_BACKOFF_MAX_MS) {
        backoffMs = FK_REJOIN_BACKOFF_MAX_MS;
      }
    }
  }
}

/* One rejoin attempt: send the request, wait for the callback, report.
   Returns true only if the stack confirmed a successful rejoin. */
static bool fkTryRejoinOnce(void) {
  xEventGroupClearBits(fkEventHandle, FK_EV_REJOIN_DONE);

  enum ZbStatusCodeT reqStatus =
      ZbStartupRejoin(stZigbeeAppInfo.pstZigbee, fkRejoinCallback, NULL);
  if (reqStatus != ZB_STATUS_SUCCESS) {
    LOG_ERROR_APP("Link: rejoin request refused (0x%02X)", reqStatus);
    return false;
  }

  EventBits_t bits =
      xEventGroupWaitBits(fkEventHandle, FK_EV_REJOIN_DONE, pdTRUE, pdFALSE,
                          pdMS_TO_TICKS(FK_REJOIN_TIMEOUT_MS));
  if ((bits & FK_EV_REJOIN_DONE) == 0U) {
    LOG_ERROR_APP("Link: rejoin timed out");
    return false;
  }
  if (fkRejoinStatus != ZB_STATUS_SUCCESS) {
    LOG_ERROR_APP("Link: rejoin failed (0x%02X)", fkRejoinStatus);
    return false;
  }

  LOG_INFO_APP("Link: rejoined, short address 0x%04X",
               ZbShortAddress(stZigbeeAppInfo.pstZigbee));
  return true;
}

/* Called by the Zigbee stack when a ZbStartupRejoin() request finishes. */
static void fkRejoinCallback(struct ZbNlmeJoinConfT *conf, void *arg) {
  (void)arg;

  fkRejoinStatus = conf->status;
  xEventGroupSetBits(fkEventHandle, FK_EV_REJOIN_DONE);
}
/* USER CODE END Application */
