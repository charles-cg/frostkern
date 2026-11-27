#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include "app_ipc.h"
#include "task.h"
#include "app_zb_rejoin.h"
#include "fake_rtos.h"
#include "log_module.h"
struct ZigBeeT {int dummy;};
enum ZbStatusCodeT {ZB_STATUS_SUCCESS=0,ZB_ZDP_STATUS_TIMEOUT=0x85};
enum zb_msg_filter_rc {ZB_MSG_CONTINUE,ZB_MSG_DISCARD};
struct ZbMsgFilterT {int dummy;};
struct ZbNlmeJoinConfT {enum ZbStatusCodeT status;};
struct ZbMsgStackEventT {int type;};
#define ZB_MSG_FILTER_STACK_EVENT 1U
#define ZB_MSG_STACK_EVENT_PARENT_LINK_FAIL 2U
#define ZB_MSG_STACK_EVENT_ATTEMPT_REJOIN 3U
#define ZB_MSG_INTERNAL_PRIO 128U
static struct ZigBeeT stack;
static struct ZbMsgFilterT filter;
static struct {struct ZigBeeT *pstZigbee;bool bInitAfterJoin;} stZigbeeAppInfo={&stack,false};
typedef struct {uint32_t token;enum ZbStatusCodeT status;} AppRejoinResult;
static AppZbRejoin slaveRejoin;
static QueueHandle_t rejoinResults;
static struct ZbMsgFilterT *parentFilter;
static unsigned requests,resets,restores,saves,filters,removed;
static unsigned freshRequests,freshCancels;
void APP_ZIGBEE_RequestFreshReports(void){++freshRequests;}
void APP_ZIGBEE_CancelFreshReports(void){++freshCancels;}
static bool inFlight,refused;
static void (*callback)(struct ZbNlmeJoinConfT *,void *);
static void *callbackArg;
void APP_ZIGBEE_LinkReady(void);
static struct ZbMsgFilterT *ZbMsgFilterRegister(struct ZigBeeT *zb,uint32_t mask,
    uint8_t priority,enum zb_msg_filter_rc (*fn)(struct ZigBeeT *,uint32_t,void *,void *),void *arg)
{assert(zb==&stack&&mask==1U&&priority==129U&&fn!=NULL&&arg==NULL);++filters;return &filter;}
static void ZbMsgFilterRemove(struct ZigBeeT *zb,struct ZbMsgFilterT *f)
{assert(zb==&stack&&f==&filter);++removed;}
static enum ZbStatusCodeT ZbStartupRejoin(struct ZigBeeT *zb,
    void (*fn)(struct ZbNlmeJoinConfT *,void *),void *arg)
{
    assert(zb==&stack&&!inFlight);++requests;
    if(refused)return ZB_ZDP_STATUS_TIMEOUT;
    inFlight=true;callback=fn;callbackArg=arg;return ZB_STATUS_SUCCESS;
}
static uint16_t ZbShortAddress(struct ZigBeeT *zb){assert(zb==&stack);return 0x1234U;}
static void AppZbPersistence_NetworkReady(void){++saves;}
static void ZbReset(struct ZigBeeT *zb){assert(zb==&stack);inFlight=false;++resets;}
static void APP_ZIGBEE_NwkFormOrJoin(void)
{++restores;APP_ZIGBEE_LinkReady();AppIpc_MarkNetworkStarted();}
void Error_Handler(void){assert(0);}
#include "rejoin_parent.inc"
#include "rejoin_callback.inc"
#include "rejoin_process.inc"
#include "rejoin_ready.inc"
static void lost(void)
{
    struct ZbMsgStackEventT event={ZB_MSG_STACK_EVENT_PARENT_LINK_FAIL};
    assert(APP_ZIGBEE_ParentEvent(&stack,1U,&event,NULL)==ZB_MSG_CONTINUE);
    APP_ZIGBEE_ProcessSlaveRejoin();
}
static void done(enum ZbStatusCodeT status)
{
    assert(inFlight);inFlight=false;
    struct ZbNlmeJoinConfT conf={status};callback(&conf,callbackArg);
    APP_ZIGBEE_ProcessSlaveRejoin();
}
int main(void)
{
    AppIpc_Init();APP_ZIGBEE_LinkReady();AppIpc_MarkNetworkStarted();
    Integration_TimeMs=10000U;lost();assert(requests==1U&&!AppIpc_NetworkStarted());
    void *oldTicket=callbackArg;
    for(unsigned i=0;i<10U;++i)APP_ZIGBEE_ProcessSlaveRejoin();
    assert(requests==1U); // Never overlap requests.
    lost();assert(requests==1U); // Duplicate link-failure events coalesce.
    done(ZB_STATUS_SUCCESS);assert(AppIpc_NetworkStarted()&&saves==1U&&freshRequests==2U&&freshCancels>=1U);
    lost();assert(requests==2U);
    struct ZbNlmeJoinConfT stale={ZB_STATUS_SUCCESS};
    APP_ZIGBEE_RejoinCallback(&stale,oldTicket);APP_ZIGBEE_ProcessSlaveRejoin();
    assert(!AppIpc_NetworkStarted()&&slaveRejoin.pending);
    done(ZB_ZDP_STATUS_TIMEOUT);
    Integration_TimeMs+=990U;APP_ZIGBEE_ProcessSlaveRejoin();assert(requests==2U);
    Integration_TimeMs+=10U;APP_ZIGBEE_ProcessSlaveRejoin();assert(requests==3U);
    // Missing confirmation requires reset/restore before a later request.
    Integration_TimeMs+=APP_ZB_REJOIN_TIMEOUT_MS;
    APP_ZIGBEE_ProcessSlaveRejoin();assert(resets==1U&&restores==1U&&AppIpc_NetworkStarted());
    assert(removed==1U&&filters==2U);
    // Immediate API failures also recover via persisted startup after 5 attempts.
    refused=true;unsigned before=requests;lost();
    while(restores<2U){Integration_TimeMs+=60000U;APP_ZIGBEE_ProcessSlaveRejoin();}
    assert(requests==before+5U&&resets==2U&&!inFlight);
    // A first uptime near rollover must not suppress the initial request.
    AppZbRejoin s={0};AppZbRejoin_Lost(&s,UINT32_MAX-100U);
    assert(AppZbRejoin_Next(&s,UINT32_MAX-100U)==APP_ZB_REJOIN_SEND);
    assert(AppZbRejoin_Next(&s,100U)==APP_ZB_REJOIN_NONE);
    assert(AppZbRejoin_Next(&s,64900U)==APP_ZB_REJOIN_RESTORE);
    Integration_FreeObjects();
    puts("PASS Slave rejoin: production callbacks/task, no overlap, backoff, late confirmations, reset/restore, event coalescing and rollover (fake SDK).");
}
