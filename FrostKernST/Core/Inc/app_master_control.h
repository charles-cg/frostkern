#ifndef APP_MASTER_CONTROL_H
#define APP_MASTER_CONTROL_H
#include <stdbool.h>
#include <stdint.h>
#include "app_fan_policy.h"
typedef struct {
    bool air_seen, air_valid;
    int16_t air_centi;
    uint32_t air_updated_ms;
    bool ac_enabled;
    uint8_t ac_requested, ac_applied;
    AppFanConfig automatic;
    AppFanResult radiator;
    int16_t input_centi;
    uint32_t input_age_ms;
    uint8_t bench_mode;
} AppMasterSnapshot;
void AppMaster_Start(void);
/* Returns UART error code. ACK means QUEUED; applied state is in 0x14. */
uint8_t AppMaster_QueueCommand(uint8_t type, const uint8_t *payload, uint8_t length);
bool AppMaster_Read(AppMasterSnapshot *out);
#endif
