#include <assert.h>
#include <stdio.h>
#include "app_zb_recovery.h"

int main(void)
{
    AppZbRecovery s;
    AppZbRecoveryPeer p[APP_ZB_RECOVERY_MAX_PEERS] = {0};
    AppZbRecovery_Init(&s);
    assert(AppZbRecovery_Next(&s, 999999U, p) == APP_ZB_RECOVERY_NONE);
    p[0].known = true;
    p[0].updated_at_ms = 1000U;
    assert(AppZbRecovery_Next(&s, 1000U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 181000U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 181001U, p) == APP_ZB_RECOVERY_OPEN);
    assert(AppZbRecovery_Next(&s, 241000U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 241001U, p) == APP_ZB_RECOVERY_OPEN);
    p[0].updated_at_ms = 242000U;
    assert(AppZbRecovery_Next(&s, 242000U, p) == APP_ZB_RECOVERY_CLOSE);
    assert(AppZbRecovery_Next(&s, 242001U, p) == APP_ZB_RECOVERY_NONE);

    /* El otro Slave sano no oculta la perdida de este. */
    p[1].known = true;
    p[1].updated_at_ms = 500000U;
    assert(AppZbRecovery_Next(&s, 500000U, p) == APP_ZB_RECOVERY_OPEN);
    p[0].updated_at_ms = 500100U;
    p[1].updated_at_ms = 1000U;
    assert(AppZbRecovery_Next(&s, 500100U, p) == APP_ZB_RECOVERY_NONE);
    p[1].updated_at_ms = 500101U;
    assert(AppZbRecovery_Next(&s, 500101U, p) == APP_ZB_RECOVERY_CLOSE);

    /* Registrado, sin primer reporte: dar tiempo a configuracion/reintentos. */
    AppZbRecovery_Init(&s);
    p[1].known = false; p[0].updated_at_ms = 0U;
    assert(AppZbRecovery_Next(&s, 1000U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 181000U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 181001U, p) == APP_ZB_RECOVERY_OPEN);

    /* Apertura manual conserva su ventana; cierre manual pausa recuperacion. */
    AppZbRecovery_ManualCommand(&s, 200000U, 180U);
    assert(AppZbRecovery_Next(&s, 379999U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 380000U, p) == APP_ZB_RECOVERY_OPEN);
    AppZbRecovery_ManualCommand(&s, 380001U, 0U);
    assert(AppZbRecovery_Next(&s, 999999U, p) == APP_ZB_RECOVERY_NONE);
    AppZbRecovery_ManualCommand(&s, 1000000U, 1U);
    assert(AppZbRecovery_Next(&s, 1000999U, p) == APP_ZB_RECOVERY_NONE);
    assert(AppZbRecovery_Next(&s, 1001000U, p) == APP_ZB_RECOVERY_OPEN);

    /* Desbordamiento de uptime de 32 bits. */
    AppZbRecovery_Init(&s);
    p[0].updated_at_ms = UINT32_MAX - 1000U;
    uint32_t now = p[0].updated_at_ms + 180001U;
    assert(AppZbRecovery_Next(&s, now, p) == APP_ZB_RECOVERY_OPEN);
    assert(AppZbRecovery_Next(&s, now + 60000U, p) == APP_ZB_RECOVERY_OPEN);
    p[0].updated_at_ms = now + 60001U;
    assert(AppZbRecovery_Next(&s, now + 60001U, p) == APP_ZB_RECOVERY_CLOSE);
    puts("PASS recovery policy: stale boundary, renewal, two Slaves, first-report grace, manual override, uint32 wrap");
    return 0;
}
