#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_ipc.h"
#include "app_zb_recovery.h"
#include "fake_rtos.h"
#include "task.h"
#include "log_module.h"

static AppZbRecovery joinRecovery;
void Error_Handler(void) { assert(0); }
static uint8_t requests[32];
static unsigned int requestCount;
static void APP_ZIGBEE_RequestPermitJoin(uint8_t seconds)
{
    assert(requestCount < sizeof(requests));
    requests[requestCount++] = seconds;
}
#include "zigbee_recovery.inc"
#include "zigbee_public_permit.inc"

int main(void)
{
    const uint64_t ieee = UINT64_C(0x0080E1FFFE32FC18);
    Integration_TimeMs = 1000U;
    AppIpc_Init();
    AppZbRecovery_Init(&joinRecovery);
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 0U);
    AppIpc_MarkNetworkStarted();
    APP_ZIGBEE_PermitJoin(180U); assert(requests[0] == 180U);
    uint8_t id = AppIpc_RegisterSlave(ieee); assert(id == 1U);
    assert(AppIpc_SaveSlaveTemperature(ieee, 2424, true));
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 1U);

    Integration_TimeMs = 181010U; /* El RTOS de prueba usa ticks de 10 ms. */
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 2U && requests[1] == 180U);
    Integration_TimeMs += 60000U;
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 3U && requests[2] == 180U);
    /* Mutex ocupado no se interpreta como una recuperacion del Slave. */
    Integration_MutexAllowed = false;
    Integration_TimeMs += 60000U;
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 3U);
    Integration_MutexAllowed = true;
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 4U);

    /* Regresa el mismo IEEE: conserva id y recibe el reporte nuevo. */
    assert(AppIpc_RegisterSlave(ieee) == id);
    assert(AppIpc_SaveSlaveTemperature(ieee, 2546, true));
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 5U && requests[4] == 0U);
    AppStatus status;
    assert(AppIpc_ReadSlaveStatus(id, &status));
    assert(status.latest_sample.temperature_centi_c == 2546);
    assert(status.updated_at_ms == Integration_TimeMs);
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == 5U);
    APP_ZIGBEE_PermitJoin(255U); assert(requestCount == 5U);

    /* Un reporte recibido con sensor invalido sigue probando comunicacion. */
    Integration_TimeMs += 180010U;
    APP_ZIGBEE_ProcessRecovery(); assert(requests[requestCount-1] == 180U);
    Integration_TimeMs += 10U;
    assert(AppIpc_SaveSlaveTemperature(ieee, 0, false));
    APP_ZIGBEE_ProcessRecovery(); assert(requests[requestCount-1] == 0U);
    APP_ZIGBEE_PermitJoin(0U);
    unsigned int count = requestCount;
    Integration_TimeMs += 999999U;
    APP_ZIGBEE_ProcessRecovery(); assert(requestCount == count);
    Integration_FreeObjects();
    puts("PASS actual Master recovery helper + real IPC: loss, renewed windows, mutex, same-IEEE return, invalid sensor report, manual pause");
    return 0;
}
