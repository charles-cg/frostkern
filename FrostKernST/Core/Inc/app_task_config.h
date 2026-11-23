/*
 * app_task_config.h
 *
 *  Created on: 19 nov 2026
 *      Author: ximef
 */


//Configuraciones estandares para master-slave

#ifndef FROSTKERN_APP_TASK_CONFIG_H
#define FROSTKERN_APP_TASK_CONFIG_H

#include "FreeRTOS.h"

/* Prioridades nativas de FreeRTOS */
#define APP_TASK_PRIO_CONTROL       32U
#define APP_TASK_PRIO_SENSOR        28U
#define APP_TASK_PRIO_ZIGBEE        24U
#define APP_TASK_PRIO_BRIDGE        20U
#define APP_TASK_PRIO_SUPERVISOR    16U

/* Tamaños de pila expresados en bytes */
#define APP_TASK_STACK_CONTROL      1024U
#define APP_TASK_STACK_SENSOR       1024U
#define APP_TASK_STACK_ZIGBEE        3072U
#define APP_TASK_STACK_BRIDGE        1536U
#define APP_TASK_STACK_SUPERVISOR    768U

/* xTaskCreate necesita palabras, no bytes */
#define APP_STACK_DEPTH(bytes) \
    (((bytes) + sizeof(StackType_t) - 1U) / sizeof(StackType_t))

#endif
