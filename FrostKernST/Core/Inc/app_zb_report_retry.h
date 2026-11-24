#ifndef FROSTKERN_APP_ZB_REPORT_RETRY_H
#define FROSTKERN_APP_ZB_REPORT_RETRY_H

#include <stdbool.h>
#include <stdint.h>

/* Pure C policy. All calls are made by ZbAppTask, never by a stack callback. */
#define APP_ZB_REPORT_RETRY_PEERS 2U
#define APP_ZB_REPORT_FIRST_DELAY_MS 20000U
#define APP_ZB_REPORT_RESPONSE_MS 20000U
#define APP_ZB_REPORT_RETRY1_MS 10000U
#define APP_ZB_REPORT_RETRY2_MS 30000U
#define APP_ZB_REPORT_MAX_ATTEMPTS 3U

typedef enum {
    APP_ZB_REPORT_EMPTY = 0,
    APP_ZB_REPORT_WAITING,
    APP_ZB_REPORT_IN_FLIGHT,
    APP_ZB_REPORT_CONFIRMED,
    APP_ZB_REPORT_FAILED
} AppZbReportRetryState;

typedef struct {
    AppZbReportRetryState state;
    uint16_t short_address;
    uint8_t attempts;
    bool failure_notice;
    uint32_t token;
    uint32_t deadline_ms;
} AppZbReportRetryPeer;

typedef struct {
    AppZbReportRetryPeer peers[APP_ZB_REPORT_RETRY_PEERS];
    uint32_t next_token;
} AppZbReportRetry;

typedef enum {
    APP_ZB_REPORT_ACTION_SEND,
    APP_ZB_REPORT_ACTION_TIMEOUT,
    APP_ZB_REPORT_ACTION_EXHAUSTED
} AppZbReportRetryActionType;

typedef struct {
    AppZbReportRetryActionType type;
    uint8_t node_id;
    uint8_t attempt;
    uint16_t short_address;
    uint32_t token;
} AppZbReportRetryAction;

void AppZbReportRetry_Init(AppZbReportRetry *manager);
/* False for an invalid argument or an already-pending identical registration.
 * New registration after CONFIRMED/FAILED starts a new bounded episode.
 */
bool AppZbReportRetry_Schedule(AppZbReportRetry *manager, uint8_t node_id,
                              uint16_t short_address, uint32_t now_ms);
/* Returns matching node 1/2, or 0 for an old/unknown response. */
uint8_t AppZbReportRetry_Complete(AppZbReportRetry *manager, uint32_t token,
                                bool accepted, uint32_t now_ms);
/* Drain actions in a while loop. SEND is reserved once, before calling the API.
 * Feed an immediate API failure back through Complete(..., false, now_ms).
 */
bool AppZbReportRetry_Next(AppZbReportRetry *manager, uint32_t now_ms,
                          AppZbReportRetryAction *action);

#endif
