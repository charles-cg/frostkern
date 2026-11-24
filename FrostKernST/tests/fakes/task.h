#ifndef TEST_TASK_H
#define TEST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
BaseType_t xTaskCreate(void (*)(void *), const char *, uint32_t,
                      void *, UBaseType_t, TaskHandle_t *);
TickType_t xTaskGetTickCount(void);
uint32_t ulTaskNotifyTake(BaseType_t, TickType_t);
void vTaskNotifyGiveFromISR(TaskHandle_t, BaseType_t *);
void vTaskDelete(TaskHandle_t);
#endif
