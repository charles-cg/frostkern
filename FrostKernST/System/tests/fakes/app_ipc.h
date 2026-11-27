#ifndef TEST_IPC_H
#define TEST_IPC_H
#include <stdbool.h>
#include "app_types.h"
#include "queue.h"
extern QueueHandle_t qZbCommand;
bool AppIpc_NetworkStarted(void);
bool AppIpc_RequestPermitJoin(uint8_t, uint8_t);
bool AppIpc_ReadSlaveStatus(uint8_t, AppStatus *);
#endif
