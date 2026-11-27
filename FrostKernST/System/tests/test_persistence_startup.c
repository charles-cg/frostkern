#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "app_role.h"
#include "app_network_config.h"
#include "app_zb_persistence.h"
#include "log_module.h"

enum ZbStartType { ZbStartTypeForm=1,ZbStartTypeJoin=3 };
static struct {
    struct ZigBeeT *pstZigbee;
    enum ZbStatusCodeT eJoinStatus;
    enum ZbStartType eStartupControl;
    uint32_t lJoinDelay;
    bool bInitAfterJoin;
} stZigbeeAppInfo;
static int stBlinckLedTimer,stZbRestartJoinTimer,NwkFormRestartWaitEndSemaphore;
static bool saved,failFirst;
static unsigned int coldCalls,restoreCalls,startCalls,resetCalls,waitCalls;
#define APP_ZIGBEE_STARTUP_WAIT_DELAY 60000U
#define osWaitForever UINT32_MAX
void Integration_Log(const char *fmt,...){(void)fmt;}
static void APP_ZIGBEE_ZbStartupCallback(enum ZbStatusCodeT status,void *arg)
{(void)arg;stZigbeeAppInfo.eJoinStatus=status;}
void AppZbPersistence_Init(struct ZigBeeT *zb){assert(zb==stZigbeeAppInfo.pstZigbee);}
void AppZbPersistence_Enable(void){}
bool AppZbPersistence_HasSaved(void){return saved;}
enum ZbStatusCodeT AppZbPersistence_Restore(struct ZbStartupT *c,
    void (*cb)(enum ZbStatusCodeT,void *),void *arg)
{
    assert(c!=NULL && saved && startCalls==0);++restoreCalls;
    cb(failFirst && restoreCalls==1?ZB_ZDP_STATUS_TIMEOUT:ZB_STATUS_SUCCESS,arg);
    return ZB_STATUS_SUCCESS;
}
static enum ZbStatusCodeT ZbStartup(struct ZigBeeT *zb,struct ZbStartupT *config,
    void (*cb)(enum ZbStatusCodeT,void *),void *arg)
{
    assert(zb==stZigbeeAppInfo.pstZigbee && config!=NULL && !saved);
    ++coldCalls;cb(ZB_STATUS_SUCCESS,arg);return ZB_STATUS_SUCCESS;
}
static void APP_ZIGBEE_GetStartupConfig(struct ZbStartupT *c){c->unused=0;}
static void APP_ZIGBEE_WaitForCallback(uint32_t ms){assert(ms==60000);}
static void UTIL_TIMER_Stop(void *timer){assert(timer==&stBlinckLedTimer);}
static void APP_ZIGBEE_ConfigMeshNetwork(void){}
static void APP_ZIGBEE_ApplicationStart(void)
{assert(stZigbeeAppInfo.eJoinStatus==ZB_STATUS_SUCCESS);++startCalls;}
static void ZbReset(struct ZigBeeT *zb)
{assert(zb==stZigbeeAppInfo.pstZigbee);++resetCalls;}
static void UTIL_TIMER_StartWithPeriod(void *timer,uint32_t ms)
{assert(timer==&stZbRestartJoinTimer && ms==10000U);}
static void osSemaphoreAcquire(int semaphore,uint32_t wait)
{assert(semaphore==NwkFormRestartWaitEndSemaphore && wait==UINT32_MAX);++waitCalls;}
#include "persistence_startup.inc"
int main(void)
{
    struct ZigBeeT zb={0};stZigbeeAppInfo.pstZigbee=&zb;
    stZigbeeAppInfo.eStartupControl=APP_IS_MASTER?ZbStartTypeForm:ZbStartTypeJoin;
    saved=false;APP_ZIGBEE_NwkFormOrJoin();
    assert(coldCalls==1 && restoreCalls==0 && startCalls==1);
    coldCalls=restoreCalls=startCalls=resetCalls=waitCalls=0;
    saved=true;failFirst=true;APP_ZIGBEE_NwkFormOrJoin();
    assert(coldCalls==0 && restoreCalls==2 && startCalls==1 && resetCalls==1 && waitCalls==1);
    puts("PASS production startup: cold path, restore callback, retry saved network after failure, no accidental new network");
    return 0;
}
