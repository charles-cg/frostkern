#ifndef FROSTKERN_APP_ZB_REJOIN_H
#define FROSTKERN_APP_ZB_REJOIN_H
#include <stdbool.h>
#include <stdint.h>
/* State belongs exclusively to ZbAppTask; callbacks only post results/events.
 * After a missing callback, reset/restore before starting another request. */
#define APP_ZB_REJOIN_TIMEOUT_MS 65000U
typedef struct {
    bool lost, pending;
    uint32_t token, next_token, due_ms;
    uint8_t failures;
} AppZbRejoin;
typedef enum { APP_ZB_REJOIN_NONE, APP_ZB_REJOIN_SEND,
               APP_ZB_REJOIN_RESTORE } AppZbRejoinAction;
static inline void AppZbRejoin_Lost(AppZbRejoin *s, uint32_t now)
{
    if (!s->lost) { s->lost = true; s->due_ms = now; s->failures = 0U; }
}
static inline void AppZbRejoin_Ready(AppZbRejoin *s)
{ s->lost = false; s->pending = false; s->token = 0U; s->failures = 0U; }
static inline bool AppZbRejoin_Complete(AppZbRejoin *s, uint32_t token,
                                      bool ok, uint32_t now)
{
    if (!s->pending || token != s->token) { return false; }
    s->pending = false;
    if (ok) { AppZbRejoin_Ready(s); }
    else {
        uint32_t delay = s->failures < 6U ? (1000U << s->failures) : 60000U;
        if (s->failures < 6U) { ++s->failures; }
        s->due_ms = now + delay;
    }
    return true;
}
static inline AppZbRejoinAction AppZbRejoin_Next(AppZbRejoin *s, uint32_t now)
{
    if (!s->lost || (int32_t)(now - s->due_ms) < 0) { return APP_ZB_REJOIN_NONE; }
    if (s->pending || s->failures >= 5U) {
        s->pending = false; s->token = 0U;
        return APP_ZB_REJOIN_RESTORE;
    }
    s->pending = true;
    if (++s->next_token == 0U) { ++s->next_token; }
    s->token = s->next_token;
    s->due_ms = now + APP_ZB_REJOIN_TIMEOUT_MS;
    return APP_ZB_REJOIN_SEND;
}
#endif
