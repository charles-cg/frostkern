#ifndef INTEGRATION_TASK_H
#define INTEGRATION_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*)(void *), const char *, uint32_t,
                      void *, UBaseType_t, TaskHandle_t *);
TickType_t xTaskGetTickCount(void);
void vTaskDelay(TickType_t);
void vTaskDelete(TaskHandle_t);
uint32_t ulTaskNotifyTake(BaseType_t, TickType_t);
void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t *);
#endif
