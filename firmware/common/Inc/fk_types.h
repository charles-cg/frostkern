#ifndef FK_TYPES_H
#define FK_TYPES_H

#include <stdint.h>

/* Events bits */
#define FK_EV_FAULT (1U << 0)
#define FK_EV_PERMIT_JOIN (1U << 1)
#define FK_EV_REPORT_NOW (1U << 2)
#define FK_EV_MODE_CHANGE (1U << 3)

/* Fault Bits */
#define FK_FAULT_OVERTEMP (1U << 0)
#define FK_FAULT_NTC (1U << 1)
#define FK_FAULT_NO_FLOW (1U << 2)

typedef enum {
    FK_MODE_IDLE,
    FK_MODE_ACTIVE
} fk_mode_t;

typedef struct {
    int16_t temperatureCentiCelsius;
    uint16_t flowCentiLpm;
    uint8_t fanDutyPercent;
    uint16_t batVoltagemV;
    uint16_t faultFlags;
    fk_mode_t mode;
    uint32_t timestamp;
    uint64_t ieeeAddress;
} fk_status_t;

#endif /* FK_TYPES_H */
