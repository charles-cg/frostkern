/*
 * app_ipc.c
 *
 *  Created on: 19 nov 2026
 *      Author: ximef
 */

#include "app_ipc.h"
#include "main.h"
#include "task.h"
#include <string.h>

/* Cantidad de mensajes que caben en estas colas */
#define CONFIG_QUEUE_LENGTH     4U
#define ZB_COMMAND_QUEUE_LENGTH 16U

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
    if (sample == NULL || qSample == NULL || gStatusMutex == NULL)
    {
        return false;
    }

    AppSample snapshot = *sample;

    if (xSemaphoreTake(gStatusMutex, pdMS_TO_TICKS(20U)) != pdTRUE)
    {
        return false;
    }

    snapshot.source_node_id = gDeviceId.node_id;
    gStatus.latest_sample = snapshot;
    gStatus.updated_at_ms =
        (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;

    xSemaphoreGive(gStatusMutex);

    /* SensorTask is the sole producer. Control consumes qSample; Zigbee
     * reads the protected snapshot without consuming the control queue.
     */
    return xQueueOverwrite(qSample, &snapshot) == pdPASS;
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

void AppIpc_MarkNetworkStopped(void)
{
    if (evSystem != NULL) {
        xEventGroupClearBits(evSystem, APP_EVENT_NETWORK_STARTED);
    }
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

bool AppIpc_RequestTemperatureReports(
    uint16_t short_address,
    uint64_t ieee_address)
{
#if APP_IS_MASTER
    if (qZbCommand == NULL || short_address >= 0xFFF8U)
    {
        return false;
    }

    AppZbCommand command = {0};

    command.type = APP_ZB_COMMAND_CONFIG_TEMP_REPORT;
    command.short_address = short_address;
    command.ieee_address = ieee_address;

    return xQueueSend(qZbCommand, &command, 0) == pdPASS;
#else
    (void)short_address;
    (void)ieee_address;

    return false;
#endif
}

uint8_t AppIpc_RegisterSlave(uint64_t ieee)
{
#if APP_IS_MASTER

    if ((ieee == 0U) ||
        (ieee == UINT64_MAX) ||
        (gStatusMutex == NULL))
    {
        return APP_NODE_UNASSIGNED;
    }

    uint8_t address[8];
    const uint8_t emptyAddress[8] = {0};

    for (uint8_t i = 0U; i < 8U; ++i)
    {
        address[i] =
            (uint8_t)(ieee >> (56U - 8U * i));
    }

    if (xSemaphoreTake(
            gStatusMutex,
            pdMS_TO_TICKS(20U)) != pdTRUE)
    {
        return APP_NODE_UNASSIGNED;
    }

    uint8_t nodeId = APP_NODE_UNASSIGNED;
    uint8_t freeSlot = APP_SLAVE_COUNT;

    for (uint8_t i = 0U; i < APP_SLAVE_COUNT; ++i)
    {
        AppStatus *entry = &gSlaveStatus[i];

        if (memcmp(
                entry->device.ieee_address,
                address,
                sizeof(address)) == 0)
        {
            nodeId = (uint8_t)(i + 1U);
            break;
        }

        if ((freeSlot == APP_SLAVE_COUNT) &&
            (memcmp(
                entry->device.ieee_address,
                emptyAddress,
                sizeof(emptyAddress)) == 0))
        {
            freeSlot = i;
        }
    }

    if ((nodeId == APP_NODE_UNASSIGNED) &&
        (freeSlot < APP_SLAVE_COUNT))
    {
        AppStatus *entry = &gSlaveStatus[freeSlot];

        nodeId = (uint8_t)(freeSlot + 1U);

        entry->device.role = APP_DEVICE_ROLE_SLAVE;
        entry->device.node_id = nodeId;

        memcpy(
            entry->device.ieee_address,
            address,
            sizeof(address));

        entry->latest_sample.source_node_id = nodeId;
    }

    xSemaphoreGive(gStatusMutex);

    return nodeId;

#else

    (void)ieee;
    return APP_NODE_UNASSIGNED;

#endif
}

bool AppIpc_SaveSlaveTemperature(
    uint64_t ieee,
    int16_t temperature,
    bool valid)
{
#if APP_IS_MASTER

    uint8_t nodeId = AppIpc_RegisterSlave(ieee);

    if (nodeId == APP_NODE_UNASSIGNED)
    {
        return false;
    }

    if (xSemaphoreTake(
            gStatusMutex,
            pdMS_TO_TICKS(20U)) != pdTRUE)
    {
        return false;
    }

    AppStatus *entry = &gSlaveStatus[nodeId - 1U];

    if (valid)
    {
        entry->latest_sample.temperature_centi_c =
            temperature;

        entry->latest_sample.valid |=
            APP_SAMPLE_VALID_TEMP;
    }
    else
    {
        entry->latest_sample.valid &=
            ~APP_SAMPLE_VALID_TEMP;
    }

    uint32_t now = (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
    entry->updated_at_ms = now;
    entry->temperature_report_at_ms = now;
    entry->reports_seen |= APP_SAMPLE_VALID_TEMP;

    xSemaphoreGive(gStatusMutex);

    return true;

#else

    (void)ieee;
    (void)temperature;
    (void)valid;

    return false;

#endif
}

bool AppIpc_SaveSlaveFlow(uint64_t ieee, uint32_t flow_ml_min, bool valid)
{
#if APP_IS_MASTER
    uint8_t nodeId = AppIpc_RegisterSlave(ieee);
    if (nodeId == APP_NODE_UNASSIGNED ||
        xSemaphoreTake(gStatusMutex, pdMS_TO_TICKS(20U)) != pdTRUE) {
        return false;
    }
    AppStatus *entry = &gSlaveStatus[nodeId - 1U];
    if (valid && flow_ml_min != UINT32_MAX) {
        entry->latest_sample.flow_ml_min = flow_ml_min;
        entry->latest_sample.valid |= APP_SAMPLE_VALID_FLOW;
    } else {
        entry->latest_sample.valid &= ~APP_SAMPLE_VALID_FLOW;
    }
    uint32_t now = (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
    entry->updated_at_ms = now;
    entry->flow_report_at_ms = now;
    entry->reports_seen |= APP_SAMPLE_VALID_FLOW;
    xSemaphoreGive(gStatusMutex);
    return true;
#else
    (void)ieee; (void)flow_ml_min; (void)valid;
    return false;
#endif
}
