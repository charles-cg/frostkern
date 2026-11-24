#ifndef FK_RTOS_H
#define FK_RTOS_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "fk_config.h"
#include "semphr.h"
#include "fk_types.h"
#include "app_zigbee.h"
#include "zigbee.h"
#include "cmsis_os2.h"

extern EventGroupHandle_t fkEventHandle;
extern volatile enum ZbStatusCodeT fkRejoinStatus;

/* Defined by ST in STM32_WPAN/App/app_zigbee.c (not in any ST header).
   Releasing it makes ST's form/join task run a full join. */
extern osSemaphoreId_t NwkFormStartSemaphore;

#endif /* FK_RTOS_H */
