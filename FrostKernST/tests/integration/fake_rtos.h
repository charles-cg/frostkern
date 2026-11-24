#ifndef INTEGRATION_FAKE_RTOS_H
#define INTEGRATION_FAKE_RTOS_H
#include <stdbool.h>
#include <stdint.h>
extern uint32_t Integration_TimeMs;
extern bool Integration_MutexAllowed;
void Integration_FreeObjects(void);
#endif
