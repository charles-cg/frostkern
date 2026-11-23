#ifndef FK_RTOS_H
#define FK_RTOS_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "fk_config.h"
#include "semphr.h"
#include "fk_types.h"

extern EventGroupHandle_t evSystem;
extern fk_status_t gStatus[FK_MAX_SLAVES];
extern SemaphoreHandle_t gStatusMutex;


#endif /* FK_RTOS_H */