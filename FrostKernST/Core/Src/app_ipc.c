/*
 * app_ipc.c
 *
 *  Created on: 19 nov 2026
 *      Author: ximef
 */

#include "app_ipc.h"
#include "main.h"
#include <string.h>

/* Cantidad de mensajes que caben en estas colas */
#define CONFIG_QUEUE_LENGTH     4U
#define ZB_COMMAND_QUEUE_LENGTH 4U

/* Objetos comunes a Master y Slave */
QueueHandle_t qZbCommand = NULL;
SemaphoreHandle_t gStatusMutex = NULL;
EventGroupHandle_t evSystem = NULL;
AppDeviceId gDeviceId;

#if APP_IS_SLAVE

QueueHandle_t qSample = NULL;
QueueHandle_t qConfig = NULL;
AppStatus gStatus;

#else

AppStatus gSlaveStatus[APP_SLAVE_COUNT];

#endif

static bool ipcInitialized = false;

void AppIpc_Init(void)
{
    /* Evitar crear los objetos dos veces */
    if (ipcInitialized) {
        return;
    }

    gStatusMutex = xSemaphoreCreateMutex();

    evSystem = xEventGroupCreate();

    qZbCommand = xQueueCreate(
        ZB_COMMAND_QUEUE_LENGTH,
        sizeof(AppZbCommand)
    );

    if (gStatusMutex == NULL ||
        evSystem == NULL ||
        qZbCommand == NULL) {
        Error_Handler();
        return;
    }

    memset(&gDeviceId, 0, sizeof(gDeviceId));

#if APP_IS_SLAVE

    gDeviceId.role = APP_DEVICE_ROLE_SLAVE;
    gDeviceId.node_id = APP_NODE_UNASSIGNED;

    qSample = xQueueCreate(1U, sizeof(AppSample));

    qConfig = xQueueCreate(
        CONFIG_QUEUE_LENGTH,
        sizeof(AppConfig)
    );

    if (qSample == NULL || qConfig == NULL) {
        Error_Handler();
        return;
    }

    memset(&gStatus, 0, sizeof(gStatus));

    gStatus.device = gDeviceId;
    gStatus.state = APP_STATE_BOOT;

    gStatus.latest_sample.source_node_id =
        APP_NODE_UNASSIGNED;

    gStatus.latest_sample.battery_percent =
        APP_BATTERY_PERCENT_UNKNOWN;

    gStatus.applied_config.target_node_id =
        APP_NODE_UNASSIGNED;

#else

    gDeviceId.role = APP_DEVICE_ROLE_MASTER;
    gDeviceId.node_id = APP_NODE_MASTER;

    memset(gSlaveStatus, 0, sizeof(gSlaveStatus));

    for (uint8_t i = 0U; i < APP_SLAVE_COUNT; i++) {
        uint8_t nodeId = (uint8_t)(i + 1U);

        gSlaveStatus[i].device.role =
            APP_DEVICE_ROLE_SLAVE;

        gSlaveStatus[i].device.node_id = nodeId;

        gSlaveStatus[i].state = APP_STATE_BOOT;

        gSlaveStatus[i].latest_sample.source_node_id =
            nodeId;

        gSlaveStatus[i].latest_sample.battery_percent =
            APP_BATTERY_PERCENT_UNKNOWN;

        gSlaveStatus[i].applied_config.target_node_id =
            APP_NODE_UNASSIGNED;
    }

#endif

    ipcInitialized = true;
}

#if APP_IS_SLAVE

bool AppIpc_PublishSample(const AppSample *sample)
{
    if (sample == NULL || qSample == NULL)
    {
        return false;
    }

    return xQueueOverwrite(qSample, sample) == pdPASS;
}

#endif

#if APP_IS_SLAVE

bool AppIpc_ReadLocalStatus(AppStatus *out)
{
    if (out == NULL || gStatusMutex == NULL)
    {
        return false;
    }

    if (xSemaphoreTake(gStatusMutex, pdMS_TO_TICKS(20U)) != pdTRUE)
    {
        return false;
    }

    *out = gStatus;

    xSemaphoreGive(gStatusMutex);

    return true;
}

#else

bool AppIpc_ReadSlaveStatus(uint8_t node_id, AppStatus *out)
{
    if (out == NULL || gStatusMutex == NULL)
    {
        return false;
    }

    if (node_id < 1U || node_id > APP_SLAVE_COUNT)
    {
        return false;
    }

    if (xSemaphoreTake(gStatusMutex, pdMS_TO_TICKS(20U)) != pdTRUE)
    {
        return false;
    }

    *out = gSlaveStatus[node_id - 1U];

    xSemaphoreGive(gStatusMutex);

    return true;
}

#endif

void AppIpc_MarkNetworkStarted(void)
{
    if (evSystem == NULL)
    {
        return;
    }

    xEventGroupSetBits(evSystem, APP_EVENT_NETWORK_STARTED);
}

bool AppIpc_NetworkStarted(void)
{
    if (evSystem == NULL)
    {
        return false;
    }

    EventBits_t bits = xEventGroupGetBits(evSystem);

    return (bits & APP_EVENT_NETWORK_STARTED) != 0U;
}


void AppIpc_SetDeviceIeee(uint64_t ieee)
{
    if (gStatusMutex == NULL)
    {
        Error_Handler();
        return;
    }

    if (xSemaphoreTake(gStatusMutex, portMAX_DELAY) != pdTRUE)
    {
        Error_Handler();
        return;
    }

    for (uint8_t i = 0U; i < 8U; i++)
    {
        gDeviceId.ieee_address[i] =
            (uint8_t)(ieee >> ((7U - i) * 8U));
    }

#if APP_IS_SLAVE
    gStatus.device = gDeviceId;
#endif

    xSemaphoreGive(gStatusMutex);
}

bool AppIpc_RequestPermitJoin(uint8_t seconds, uint8_t seq)
{

#if APP_IS_MASTER

    if (qZbCommand == NULL || seconds > 254U)
    {
        return false;
    }

    if (!AppIpc_NetworkStarted())
    {
        return false;
    }

    AppZbCommand command = {0};

    command.type = APP_ZB_COMMAND_PERMIT_JOIN;
    command.seq = seq;
    command.permit_seconds = seconds;

    return xQueueSend(qZbCommand, &command, 0U) == pdPASS;

#else

    (void)seconds;
    (void)seq;

    return false;

#endif
}
