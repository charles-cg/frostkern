#include "app_fan_policy.h"
bool AppFanConfig_Valid(const AppFanConfig *c)
{
    return c != 0 && c->target_ieee != 0U && c->target_ieee != UINT64_MAX &&
        c->off_centi >= -4000 && c->full_centi <= 12500 &&
        c->off_centi < c->on_centi && c->on_centi < c->full_centi &&
        c->minimum_percent >= 1U && c->minimum_percent <= 100U;
}
AppFanResult AppFan_Evaluate(const AppFanConfig *c, bool was_running,
                            bool input_valid, int16_t centi, uint32_t age_ms)
{
    AppFanResult r={false,0U,APP_FAN_DISABLED};
    if (c == 0 || !c->enabled) { return r; }
    /* Fail to full cooling, never to an arbitrary old temperature. */
    if (!AppFanConfig_Valid(c) || !input_valid || age_ms > APP_FAN_INPUT_MAX_AGE_MS ||
        centi < -4000 || centi > 12500) {
        r.running=true; r.duty=100U; r.reason=APP_FAN_NO_FRESH_INPUT; return r;
    }
    if (centi <= c->off_centi || (!was_running && centi < c->on_centi)) {
        r.reason=APP_FAN_COOL; return r;
    }
    r.running=true; r.reason=APP_FAN_REGULATING;
    if (centi >= c->full_centi) { r.duty=100U; }
    else if (centi <= c->on_centi) { r.duty=c->minimum_percent; }
    else {
        uint32_t span=(uint32_t)(c->full_centi-c->on_centi);
        uint32_t rise=(uint32_t)(centi-c->on_centi)*(100U-c->minimum_percent);
        r.duty=(uint8_t)(c->minimum_percent+(rise+span/2U)/span);
    }
    return r;
}
