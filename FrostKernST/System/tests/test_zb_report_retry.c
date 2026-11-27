#include "app_zb_report_retry.h"
#include <assert.h>
#include <stdio.h>

int main(void)
{
    AppZbReportRetry manager;
    AppZbReportRetryAction action;
    AppZbReportRetry_Init(&manager);
    assert(!AppZbReportRetry_Next(&manager, 0U, &action));
    assert(!AppZbReportRetry_Schedule(&manager, 0U, 1U, 0U));
    assert(!AppZbReportRetry_Schedule(&manager, 3U, 1U, 0U));
    assert(!AppZbReportRetry_Schedule(&manager, 1U, 0U, 0U));
    assert(!AppZbReportRetry_Schedule(&manager, 1U, 0xFFFFU, 0U));
    assert(!AppZbReportRetry_Complete(&manager, 0U, true, 0U));

    assert(AppZbReportRetry_Schedule(&manager, 1U, 0x1111U, 0U));
    assert(!AppZbReportRetry_Schedule(&manager, 1U, 0x1111U, 5000U));
    assert(!AppZbReportRetry_Next(&manager, 19999U, &action));
    assert(AppZbReportRetry_Next(&manager, 20000U, &action));
    assert(action.type == APP_ZB_REPORT_ACTION_SEND && action.attempt == 1U);
    uint32_t first = action.token;
    assert(!AppZbReportRetry_Next(&manager, 20000U, &action));
    assert(AppZbReportRetry_Complete(&manager, first, false, 20000U) == 1U);
    assert(!AppZbReportRetry_Complete(&manager, first, true, 20001U));
    assert(!AppZbReportRetry_Next(&manager, 29999U, &action));
    assert(AppZbReportRetry_Next(&manager, 30000U, &action));
    assert(action.attempt == 2U && action.token != first);
    uint32_t second = action.token;
    assert(AppZbReportRetry_Complete(&manager, second, false, 31000U) == 1U);
    assert(!AppZbReportRetry_Next(&manager, 60999U, &action));
    assert(AppZbReportRetry_Next(&manager, 61000U, &action));
    assert(action.attempt == 3U);
    assert(AppZbReportRetry_Complete(&manager, action.token, false, 62000U) == 1U);
    assert(AppZbReportRetry_Next(&manager, 62000U, &action));
    assert(action.type == APP_ZB_REPORT_ACTION_EXHAUSTED);
    assert(!AppZbReportRetry_Next(&manager, 1000000U, &action));
    assert(manager.peers[0].attempts == 3U);

    /* No callback: timeout, retry, and rejection of a late success. */
    AppZbReportRetry_Init(&manager);
    assert(AppZbReportRetry_Schedule(&manager, 1U, 0x1111U, 0U));
    assert(AppZbReportRetry_Next(&manager, 20000U, &action));
    first = action.token;
    assert(!AppZbReportRetry_Next(&manager, 39999U, &action));
    assert(AppZbReportRetry_Next(&manager, 40000U, &action));
    assert(action.type == APP_ZB_REPORT_ACTION_TIMEOUT);
    assert(!AppZbReportRetry_Complete(&manager, first, true, 40001U));
    assert(!AppZbReportRetry_Next(&manager, 49999U, &action));
    assert(AppZbReportRetry_Next(&manager, 50000U, &action));
    assert(action.attempt == 2U);
    assert(AppZbReportRetry_Complete(&manager, action.token, true, 50050U) == 1U);
    assert(manager.peers[0].state == APP_ZB_REPORT_CONFIRMED);
    assert(!AppZbReportRetry_Next(&manager, 1000000U, &action));

    /* Each Slave has its own retry state, including simultaneous requests. */
    AppZbReportRetry_Init(&manager);
    assert(AppZbReportRetry_Schedule(&manager, 1U, 0x1111U, 0U));
    assert(AppZbReportRetry_Schedule(&manager, 2U, 0x2222U, 0U));
    assert(AppZbReportRetry_Next(&manager, 20000U, &action));
    assert(action.node_id == 1U); first = action.token;
    assert(AppZbReportRetry_Next(&manager, 20000U, &action));
    assert(action.node_id == 2U && action.token != first); second = action.token;
    assert(!AppZbReportRetry_Schedule(&manager, 1U, 0x1111U, 20001U));
    assert(AppZbReportRetry_Complete(&manager, first, false, 20001U) == 1U);
    assert(AppZbReportRetry_Complete(&manager, second, true, 20002U) == 2U);
    assert(!AppZbReportRetry_Next(&manager, 30000U, &action));
    assert(AppZbReportRetry_Next(&manager, 30001U, &action));
    assert(action.node_id == 1U && action.attempt == 2U);

    /* Rejoin with changed short address invalidates the old request ticket. */
    first = action.token;
    assert(AppZbReportRetry_Schedule(&manager, 1U, 0x3333U, 30002U));
    assert(!AppZbReportRetry_Complete(&manager, first, true, 30003U));
    assert(!AppZbReportRetry_Next(&manager, 50001U, &action));
    assert(AppZbReportRetry_Next(&manager, 50002U, &action));
    assert(action.short_address == 0x3333U && action.attempt == 1U);

    /* All three response timeouts eventually produce one terminal notice. */
    AppZbReportRetry_Init(&manager);
    assert(AppZbReportRetry_Schedule(&manager, 1U, 1U, 0U));
    const uint32_t sends[] = {20000U, 50000U, 100000U};
    for (unsigned int i = 0; i < 3U; ++i) {
        assert(AppZbReportRetry_Next(&manager, sends[i], &action));
        assert(action.type == APP_ZB_REPORT_ACTION_SEND && action.attempt == i + 1U);
        assert(AppZbReportRetry_Next(&manager, sends[i] + 20000U, &action));
        assert(action.type == APP_ZB_REPORT_ACTION_TIMEOUT);
    }
    assert(AppZbReportRetry_Next(&manager, 120000U, &action));
    assert(action.type == APP_ZB_REPORT_ACTION_EXHAUSTED);
    assert(!AppZbReportRetry_Next(&manager, 120000U, &action));
    assert(!AppZbReportRetry_Next(&manager, 123456789U, &action));
    assert(AppZbReportRetry_Schedule(&manager, 1U, 1U, 120001U));
    assert(!AppZbReportRetry_Next(&manager, 140000U, &action));
    assert(AppZbReportRetry_Next(&manager, 140001U, &action));
    assert(action.attempt == 1U);

    /* The millisecond clock and request token both handle uint32 rollover. */
    AppZbReportRetry_Init(&manager);
    uint32_t start = UINT32_MAX - 9999U;
    assert(AppZbReportRetry_Schedule(&manager, 1U, 1U, start));
    assert(!AppZbReportRetry_Next(&manager, 9999U, &action));
    manager.next_token = UINT32_MAX;
    assert(AppZbReportRetry_Next(&manager, 10000U, &action));
    assert(action.token == 1U);
    assert(AppZbReportRetry_Complete(&manager, 1U, true, 10001U) == 1U);
    puts("Zigbee report retry: first delay, bounded retries, confirmation, two Slaves, timeouts, duplicate joins, stale replies, rejoin and rollover passed.");
    return 0;
}
