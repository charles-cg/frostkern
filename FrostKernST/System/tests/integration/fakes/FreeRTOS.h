#ifndef INTEGRATION_FREERTOS_H
#define INTEGRATION_FREERTOS_H
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef uint32_t StackType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define configTICK_RATE_HZ 100U
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms) / 10U)
#define portTICK_PERIOD_MS 10U
#define portMAX_DELAY UINT32_MAX
#define portYIELD_FROM_ISR(value) ((void)(value))
#endif
