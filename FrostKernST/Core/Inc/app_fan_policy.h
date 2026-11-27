#ifndef APP_FAN_POLICY_H
#define APP_FAN_POLICY_H
#include <stdbool.h>
#include <stdint.h>
/* Pure policy, no HAL/RTOS. Calibration is deliberately disabled at boot. */
#define APP_FAN_INPUT_MAX_AGE_MS 90000U
#define APP_FAN_BENCH_LEASE_MS 5000U
#define APP_FAN_PWM_HZ 1000U

#define APP_FAN_DEFAULT_TARGET_IEEE UINT64_C(0x0080E1FFFE32FC18)
typedef struct {
    bool enabled;
    uint64_t target_ieee;
    int16_t off_centi, on_centi, full_centi;
    uint8_t minimum_percent;
} AppFanConfig;
typedef enum {
    APP_FAN_DISABLED=0, APP_FAN_COOL=1, APP_FAN_REGULATING=2,
    APP_FAN_NO_FRESH_INPUT=3, APP_FAN_BENCH=4
} AppFanReason;
typedef struct { bool running; uint8_t duty; AppFanReason reason; } AppFanResult;
bool AppFanConfig_Valid(const AppFanConfig *c);
AppFanResult AppFan_Evaluate(const AppFanConfig *c, bool was_running,
                            bool input_valid, int16_t centi, uint32_t age_ms);
#endif
