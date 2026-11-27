#ifndef APP_FAN_PWM_H
#define APP_FAN_PWM_H
#include <stdint.h>
void AppFanPwm_Init(void);
void AppFanPwm_Set(uint8_t ac_percent, uint8_t radiator_percent);
#endif
