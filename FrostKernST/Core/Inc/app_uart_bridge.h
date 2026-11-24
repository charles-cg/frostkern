#ifndef FROSTKERN_APP_UART_BRIDGE_H
#define FROSTKERN_APP_UART_BRIDGE_H

/* Call once in MX_FREERTOS_Init, after AppIpc_Init, before scheduler start.
 * Creates BridgeTask only for ROLE_MASTER. ROLE_SLAVE is a no-op.
 */
void AppUartBridge_Start(void);

#endif
