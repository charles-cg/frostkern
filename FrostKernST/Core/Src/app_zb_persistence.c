#include "app_zb_persistence.h"
#include "app_persist_flash.h"
#include "app_network_config.h"
#include "app_ipc.h"
#include "task.h"
#include "log_module.h"
#include <stdlib.h>
#include <string.h>

static struct ZigBeeT *stack;
static AppPersistIo io;
static AppPersistIdentity identity;
static AppPersistRecord latest;
static bool available,ready,triedSave;
static uint32_t lastAttempt;
static volatile uint32_t notification;
static uint32_t flushed;

static uint64_t Ieee(const uint8_t *p)
{
    uint64_t n=0;
    for(unsigned i=0;i<8;++i)n=(n<<8)|p[i];
    return n;
}
static uint32_t Now(void)
{return (uint32_t)xTaskGetTickCount()*(uint32_t)portTICK_PERIOD_MS;}
static void Notify(struct ZigBeeT *zb,void *arg)
{
    (void)zb;(void)arg;
    /* No Flash ni malloc en el callback de la pila; trabajo en ZbAppTask. */
    ++notification;
}
void AppZbPersistence_Enable(void)
{
    if(available && !ZbPersistNotifyRegister(stack,Notify,NULL))
        LOG_ERROR_APP("[ZB-PERSIST] could not register persistence notifications");
}
void AppZbPersistence_Init(struct ZigBeeT *zb)
{
    stack=zb;ready=false;triedSave=false;notification=1U;flushed=0U;
    memset(&latest,0,sizeof(latest));
    identity.ieee=ZbExtendedAddress(zb);
    identity.role=APP_IS_MASTER?1U:2U;
    identity.channel=APP_NETWORK_CHANNEL;
    identity.format=APP_PERSIST_FORMAT_ID;
    available=AppPersistFlash_Open(&io);
    if(!available) {
        LOG_ERROR_APP("[ZB-PERSIST] invalid Flash/linker geometry; persistence unavailable");
        return;
    }
#if APP_ZB_PERSIST_ERASE_ON_BOOT
    if(AppPersistFlash_Begin()) {
        bool erased=true;
        for(uint32_t p=0;p<APP_PERSIST_PAGE_COUNT;++p)
            if(!io.erase(io.context,p))erased=false;
        AppPersistFlash_End();
        LOG_INFO_APP("[ZB-PERSIST] factory erase result=%u",(unsigned int)erased);
        if(!erased){available=false;return;}
    } else {available=false;return;}
#endif
    if(AppPersistStore_Load(&io,&identity,&latest)) {
        LOG_INFO_APP("[ZB-PERSIST] saved network found: %lu bytes, generation=%lu",
            (unsigned long)latest.length,(unsigned long)latest.sequence);
    } else {
        LOG_INFO_APP("[ZB-PERSIST] no valid saved network; normal form/join");
    }
    AppZbPersistence_Enable();
}
bool AppZbPersistence_HasSaved(void)
{return available && latest.valid;}
enum ZbStatusCodeT AppZbPersistence_Restore(struct ZbStartupT *config,
    void (*callback)(enum ZbStatusCodeT,void *),void *arg)
{
    if(!AppZbPersistence_HasSaved())return ZB_ZDP_STATUS_TIMEOUT;
    LOG_INFO_APP("[ZB-PERSIST] restoring saved network");
    return ZbStartupPersist(stack,AppPersistFlash_Data(latest.offset+APP_PERSIST_DATA),
        latest.length,config,callback,arg);
}
static bool CapturePeers(uint8_t peers[16])
{
    memset(peers,0,16);
#if APP_IS_MASTER
    _Static_assert(APP_SLAVE_COUNT==2U,"Actualizar formato de persistencia para mas Slaves");
    for(uint8_t id=1U;id<=APP_SLAVE_COUNT;++id) {
        AppStatus status;
        if(!AppIpc_ReadSlaveStatus(id,&status))return false;
        memcpy(peers+(id-1U)*8U,status.device.ieee_address,8);
    }
#endif
    return true;
}
static void Save(bool force)
{
    if(!available||!ready)return;
    uint8_t peers[16];if(!CapturePeers(peers))return;
    uint32_t generation=notification;
    if(!force && generation==flushed && latest.valid &&
       memcmp(peers,latest.peers,16)==0)return;
    if(!force && triedSave && (uint32_t)(Now()-lastAttempt)<5000U)return;
    triedSave=true;lastAttempt=Now();
    unsigned int length=ZbPersistGet(stack,NULL,0);
    if(length==0U||length>APP_PERSIST_MAX_DATA) {
        LOG_ERROR_APP("[ZB-PERSIST] invalid snapshot length: %u",length);return;
    }
    uint8_t *data=malloc(length);
    if(data==NULL){LOG_ERROR_APP("[ZB-PERSIST] snapshot allocation failed");return;}
    AppPersistResult result=APP_PERSIST_ERROR;
    if(ZbPersistGet(stack,data,length)==length && AppPersistFlash_Begin()) {
        result=AppPersistStore_Save(&io,&identity,peers,data,length,&latest);
        AppPersistFlash_End();
    }
    free(data);
    if(result!=APP_PERSIST_ERROR) {
        flushed=generation;
        if(result==APP_PERSIST_SAVED)
            LOG_INFO_APP("[ZB-PERSIST] saved: %lu bytes, generation=%lu",
                (unsigned long)latest.length,(unsigned long)latest.sequence);
    } else LOG_ERROR_APP("[ZB-PERSIST] save failed; will retry");
}
void AppZbPersistence_NetworkReady(void)
{
#if APP_IS_MASTER
    if(latest.valid) {
        for(unsigned i=0;i<2;++i) {
            uint64_t ieee=Ieee(latest.peers+i*8U);
            if(ieee!=0U && ieee!=UINT64_MAX && AppIpc_RegisterSlave(ieee)==APP_NODE_UNASSIGNED)
                LOG_ERROR_APP("[ZB-PERSIST] could not restore Slave registry");
        }
    }
#else
    (void)Ieee;
#endif
    ready=true;
    AppZbPersistence_Enable();
    ++notification;
    Save(true);
}
void AppZbPersistence_Process(void)
{Save(false);}
