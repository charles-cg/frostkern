#include "app_zb_report_retry.h"
#include <stddef.h>
#include <string.h>

static bool Due(uint32_t now_ms, uint32_t deadline_ms)
{
    /* All configured delays are much less than half the uint32 clock range. */
    return (uint32_t)(now_ms - deadline_ms) < UINT32_C(0x80000000);
}

static void FailedAttempt(AppZbReportRetryPeer *peer, uint32_t now_ms)
{
    peer->token = 0U;
    if (peer->attempts >= APP_ZB_REPORT_MAX_ATTEMPTS) {
        peer->state = APP_ZB_REPORT_FAILED;
        peer->failure_notice = true;
    } else {
        peer->state = APP_ZB_REPORT_WAITING;
        peer->deadline_ms = now_ms + (peer->attempts == 1U
            ? APP_ZB_REPORT_RETRY1_MS : APP_ZB_REPORT_RETRY2_MS);
    }
}

void AppZbReportRetry_Init(AppZbReportRetry *manager)
{
    memset(manager, 0, sizeof(*manager));
}

bool AppZbReportRetry_Schedule(AppZbReportRetry *manager, uint8_t node_id,
                              uint16_t short_address, uint32_t now_ms)
{
    if (node_id == 0U || node_id > APP_ZB_REPORT_RETRY_PEERS ||
        short_address == 0U || short_address >= 0xFFF8U) {
        return false;
    }
    AppZbReportRetryPeer *peer = &manager->peers[node_id - 1U];
    if ((peer->state == APP_ZB_REPORT_WAITING ||
         peer->state == APP_ZB_REPORT_IN_FLIGHT) &&
        peer->short_address == short_address) {
        return false;
    }
    /* Invalidates any older in-flight token on a change of network address. */
    memset(peer, 0, sizeof(*peer));
    peer->state = APP_ZB_REPORT_WAITING;
    peer->short_address = short_address;
    peer->deadline_ms = now_ms + APP_ZB_REPORT_FIRST_DELAY_MS;
    return true;
}

uint8_t AppZbReportRetry_Complete(AppZbReportRetry *manager, uint32_t token,
                                bool accepted, uint32_t now_ms)
{
    if (token == 0U) {
        return 0U;
    }
    for (uint8_t i = 0U; i < APP_ZB_REPORT_RETRY_PEERS; ++i) {
        AppZbReportRetryPeer *peer = &manager->peers[i];
        if (peer->state != APP_ZB_REPORT_IN_FLIGHT || peer->token != token) {
            continue;
        }
        if (accepted) {
            peer->state = APP_ZB_REPORT_CONFIRMED;
            peer->token = 0U;
        } else {
            FailedAttempt(peer, now_ms);
        }
        return (uint8_t)(i + 1U);
    }
    return 0U;
}

static void SetAction(AppZbReportRetryAction *action,
                       AppZbReportRetryActionType type,
                       uint8_t index, const AppZbReportRetryPeer *peer)
{
    action->type = type;
    action->node_id = (uint8_t)(index + 1U);
    action->attempt = peer->attempts;
    action->short_address = peer->short_address;
    action->token = peer->token;
}

bool AppZbReportRetry_Next(AppZbReportRetry *manager, uint32_t now_ms,
                          AppZbReportRetryAction *action)
{
    for (uint8_t i = 0U; i < APP_ZB_REPORT_RETRY_PEERS; ++i) {
        AppZbReportRetryPeer *peer = &manager->peers[i];
        if (peer->state == APP_ZB_REPORT_IN_FLIGHT && Due(now_ms, peer->deadline_ms)) {
            SetAction(action, APP_ZB_REPORT_ACTION_TIMEOUT, i, peer);
            FailedAttempt(peer, now_ms);
            return true;
        }
        if (peer->failure_notice) {
            peer->failure_notice = false;
            SetAction(action, APP_ZB_REPORT_ACTION_EXHAUSTED, i, peer);
            return true;
        }
        if (peer->state != APP_ZB_REPORT_WAITING || !Due(now_ms, peer->deadline_ms)) {
            continue;
        }
        ++manager->next_token;
        if (manager->next_token == 0U) {
            ++manager->next_token;
        }
        peer->token = manager->next_token;
        ++peer->attempts;
        peer->state = APP_ZB_REPORT_IN_FLIGHT;
        peer->deadline_ms = now_ms + APP_ZB_REPORT_RESPONSE_MS;
        SetAction(action, APP_ZB_REPORT_ACTION_SEND, i, peer);
        return true;
    }
    return false;
}
