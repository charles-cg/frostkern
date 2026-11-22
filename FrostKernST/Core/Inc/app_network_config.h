#ifndef FROSTKERN_APP_NETWORK_CONFIG_H
#define FROSTKERN_APP_NETWORK_CONFIG_H

#include <stdint.h>
#include "app_role.h"

/* Bring-up de red: mismos parametros en los tres WBA55. */
#define APP_NETWORK_CHANNEL                  14U
#define APP_NETWORK_ENDPOINT                 17U
#define APP_NETWORK_SED_TIMEOUT_CODE         1U /* Timeout 120 s; NO es intervalo de polling. */

/* Solo para la prueba inicial: el coordinador admite altas durante 180 s
 * despues de formar la red. El boton/UART se integraran en la fase de tareas.
 * 0 desactiva la ventana automatica. No se abre indefinidamente.
 */
#define APP_NETWORK_OPEN_JOIN_ON_BOOT        1U
#define APP_NETWORK_PERMIT_JOIN_SECONDS      180U

#if APP_IS_MASTER
#define APP_NETWORK_ROLE_NAME                "MASTER / COORDINATOR / FFD"
#else
#define APP_NETWORK_ROLE_NAME                "SLAVE / SLEEPY END DEVICE / RFD"
#endif

#if APP_NETWORK_CHANNEL < 11U || APP_NETWORK_CHANNEL > 26U
#error "Canal Zigbee fuera de 11..26"
#endif
#if APP_NETWORK_PERMIT_JOIN_SECONDS > 254U
#error "La ventana de altas debe ser finita (0..254 segundos)"
#endif

/* Pausa DESPUES de un intento fallido; ZbStartup puede tardar hasta 60 s.
 * El indice empieza en 0 y se satura en 2 en el llamador.
 */
static inline uint32_t AppNetwork_JoinRetryDelayMs(uint32_t failed_attempt_index)
{
    if (failed_attempt_index == 0U) {
        return 10000U;
    }
    if (failed_attempt_index == 1U) {
        return 60000U;
    }
    return 300000U;
}

#endif /* FROSTKERN_APP_NETWORK_CONFIG_H */
