#ifndef FK_RTOS_H
#define FK_RTOS_H

#include "FreeRTOS.h"
#include "event_groups.h"
#include "fk_config.h"
#include "semphr.h"
#include "fk_types.h"
#include "app_zigbee.h"
#include "zigbee.h"

extern EventGroupHandle_t fkEventHandle;
extern volatile enum ZbStatusCodeT fkRejoinStatus;

#endif /* FK_RTOS_H */
