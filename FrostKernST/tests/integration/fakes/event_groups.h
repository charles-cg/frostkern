#ifndef INTEGRATION_EVENTS_H
#define INTEGRATION_EVENTS_H
#include "FreeRTOS.h"
typedef uint32_t EventBits_t;
typedef void *EventGroupHandle_t;
EventGroupHandle_t xEventGroupCreate(void);
EventBits_t xEventGroupGetBits(EventGroupHandle_t);
EventBits_t xEventGroupSetBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupClearBits(EventGroupHandle_t, EventBits_t);
EventBits_t xEventGroupWaitBits(EventGroupHandle_t, EventBits_t, BaseType_t,
                                    BaseType_t, TickType_t);
#endif
