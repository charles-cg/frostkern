/*
 * app_ipc.h
 *
 *  Created on: 19 nov 2026
 *      Author: ximef
 */

//Configuración de la configuración y la información intercambiada

#ifndef FROSTKERN_APP_IPC_H
#define FROSTKERN_APP_IPC_H

#include <stdbool.h>
#include <stdint.h>

#include "app_role.h"
#include "app_types.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"

/* Avisos entre tareas */
#define APP_EVENT_FAULT           (1UL << 0)
#define APP_EVENT_BUTTON          (1UL << 1)
#define APP_EVENT_REPORT_NOW      (1UL << 2)
#define APP_EVENT_MODE_CHANGE     (1UL << 3)
#define APP_EVENT_NETWORK_STARTED (1UL << 4)
#define APP_ZB_COMMAND_PERMIT_JOIN 1U
#define APP_ZB_COMMAND_CONFIG_TEMP_REPORT 2U
#define APP_ZB_COMMAND_TEMP_RECEIVED 3U

/* Comando de BridgeTask hacia ZbAppTask */
typedef struct
{
    uint8_t type;
    uint8_t seq;
    uint8_t permit_seconds;

    uint16_t short_address;
    uint64_t ieee_address;
    int16_t temperature_centi_c;
    bool temperature_valid;
} AppZbCommand;

/* Objetos presentes en ambos roles */
extern QueueHandle_t qZbCommand;
extern SemaphoreHandle_t gStatusMutex;
extern EventGroupHandle_t evSystem;
extern AppDeviceId gDeviceId;

#if APP_IS_SLAVE

/* Sensores → Control: última medición */
extern QueueHandle_t qSample;

/* Zigbee → Control: nueva configuración */
extern QueueHandle_t qConfig;

/* Estado local del Slave */
extern AppStatus gStatus;

bool AppIpc_PublishSample(const AppSample *sample);
bool AppIpc_ReadLocalStatus(AppStatus *out);

#else

/* Master guarda un estado por cada Slave */
extern AppStatus gSlaveStatus[APP_SLAVE_COUNT];

bool AppIpc_ReadSlaveStatus(uint8_t node_id, AppStatus *out);

#endif

void AppIpc_Init(void);
void AppIpc_SetDeviceIeee(uint64_t ieee);
bool AppIpc_NetworkStarted(void);
void AppIpc_MarkNetworkStarted(void);

bool AppIpc_RequestPermitJoin(uint8_t seconds, uint8_t seq);

bool AppIpc_RequestTemperatureReports(
    uint16_t short_address,
    uint64_t ieee_address);

uint8_t AppIpc_RegisterSlave(uint64_t ieee);

bool AppIpc_SaveSlaveTemperature(
    uint64_t ieee,
    int16_t temperature,
    bool valid);
#endif
