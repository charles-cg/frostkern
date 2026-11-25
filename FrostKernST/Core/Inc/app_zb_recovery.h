#ifndef FROSTKERN_APP_ZB_RECOVERY_H
#define FROSTKERN_APP_ZB_RECOVERY_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "app_network_config.h"

#define APP_ZB_RECOVERY_MAX_PEERS 2U

typedef struct {
    bool known;
    uint32_t updated_at_ms; /* 0: no se ha recibido el primer reporte. */
} AppZbRecoveryPeer;

typedef enum {
    APP_ZB_RECOVERY_NONE,
    APP_ZB_RECOVERY_OPEN,
    APP_ZB_RECOVERY_CLOSE
} AppZbRecoveryAction;

typedef struct {
    bool observed[APP_ZB_RECOVERY_MAX_PEERS];
    uint32_t first_seen_ms[APP_ZB_RECOVERY_MAX_PEERS];
    bool active;
    bool manual_pause;
    uint32_t manual_start_ms;
    uint32_t manual_hold_ms;
    uint32_t last_request_ms;
} AppZbRecovery;

static inline void AppZbRecovery_Init(AppZbRecovery *state)
{
    memset(state, 0, sizeof(*state));
}

/* Un cierre UART explicito pausa la reapertura automatica hasta otra apertura
 * manual o un reinicio del Master. Una apertura manual conserva su duracion.
 */
static inline void AppZbRecovery_ManualCommand(
    AppZbRecovery *state, uint32_t now_ms, uint8_t seconds)
{
    state->active = false;
    state->manual_pause = seconds == 0U;
    state->manual_start_ms = now_ms;
    state->manual_hold_ms = (uint32_t)seconds * 1000U;
}

/* Se llama solamente con una instantanea completa del registro IPC.
 * La resta uint32 conserva el comportamiento al desbordarse el reloj.
 * La accion indica una solicitud: la confirmacion de radio es asincrona.
 */
static inline AppZbRecoveryAction AppZbRecovery_Next(
    AppZbRecovery *state, uint32_t now_ms,
    const AppZbRecoveryPeer peers[APP_ZB_RECOVERY_MAX_PEERS])
{
    bool missing = false;
    for (unsigned int i = 0U; i < APP_ZB_RECOVERY_MAX_PEERS; ++i) {
        if (!peers[i].known) {
            state->observed[i] = false;
            continue;
        }
        if (!state->observed[i]) {
            state->observed[i] = true;
            state->first_seen_ms[i] = now_ms;
        }
        if (peers[i].updated_at_ms != 0U) {
            if ((uint32_t)(now_ms - peers[i].updated_at_ms) >
                    APP_NETWORK_RECOVERY_STALE_MS) {
                missing = true;
            }
        } else if ((uint32_t)(now_ms - state->first_seen_ms[i]) >
                       APP_NETWORK_RECOVERY_FIRST_REPORT_MS) {
            missing = true;
        }
    }

    if (state->manual_pause ||
        (uint32_t)(now_ms - state->manual_start_ms) < state->manual_hold_ms) {
        return APP_ZB_RECOVERY_NONE;
    }
    if (missing) {
        if (!state->active || (uint32_t)(now_ms - state->last_request_ms) >=
                                 APP_NETWORK_RECOVERY_REFRESH_MS) {
            state->active = true;
            state->last_request_ms = now_ms;
            return APP_ZB_RECOVERY_OPEN;
        }
    } else if (state->active) {
        state->active = false;
        return APP_ZB_RECOVERY_CLOSE;
    }
    return APP_ZB_RECOVERY_NONE;
}

#endif
