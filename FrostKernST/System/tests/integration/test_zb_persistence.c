#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "app_zb_persistence.h"
#include "app_persist_flash.h"
#include "app_ipc.h"
#include "fake_rtos.h"

static uint8_t flash[APP_PERSIST_SIZE];
static bool writing;
static unsigned int programs;
static void (*notify)(struct ZigBeeT *,void *);
static struct ZigBeeT zb;
static uint8_t stackData[91];
static unsigned snapshots,restores;
static bool failGet;
void Error_Handler(void){assert(0);}
static bool Read(void *ctx,uint32_t off,void *out,size_t n)
{(void)ctx;assert(off+n<=sizeof(flash));memcpy(out,flash+off,n);return true;}
static bool Erase(void *ctx,uint32_t page)
{(void)ctx;assert(writing && page<3);memset(flash+page*APP_PERSIST_PAGE_SIZE,0xFF,APP_PERSIST_PAGE_SIZE);return true;}
static bool Program(void *ctx,uint32_t off,const uint8_t *p)
{(void)ctx;assert(writing && off%16==0 && off+16<=sizeof(flash));memcpy(flash+off,p,16);++programs;return true;}
bool AppPersistFlash_Open(AppPersistIo *io)
{*io=(AppPersistIo){NULL,Read,Erase,Program};return true;}
bool AppPersistFlash_Begin(void){assert(!writing);writing=true;return true;}
void AppPersistFlash_End(void){assert(writing);writing=false;}
const uint8_t *AppPersistFlash_Data(uint32_t off){assert(off<sizeof(flash));return flash+off;}
uint64_t ZbExtendedAddress(struct ZigBeeT *stack)
{assert(stack==&zb);return UINT64_C(0x0080E1FFFE32FCE9);}
bool ZbPersistNotifyRegister(struct ZigBeeT *stack,void (*fn)(struct ZigBeeT *,void *),void *arg)
{assert(stack==&zb && arg==NULL);notify=fn;return true;}
unsigned int ZbPersistGet(struct ZigBeeT *stack,uint8_t *out,unsigned int size)
{
    assert(stack==&zb);++snapshots;
    if(failGet)return 0;
    if(out){assert(size>=sizeof(stackData));memcpy(out,stackData,sizeof(stackData));}
    return sizeof(stackData);
}
static void Restored(enum ZbStatusCodeT status,void *arg)
{assert(status==ZB_STATUS_SUCCESS && arg==&restores);++restores;}
enum ZbStatusCodeT ZbStartupPersist(struct ZigBeeT *stack,const void *p,unsigned int n,
    struct ZbStartupT *config,void (*cb)(enum ZbStatusCodeT,void *),void *arg)
{
    assert(stack==&zb && config!=NULL && n==sizeof(stackData));
    assert(memcmp(p,stackData,n)==0);cb(ZB_STATUS_SUCCESS,arg);return ZB_STATUS_SUCCESS;
}
int main(void)
{
    memset(flash,0xFF,sizeof(flash));memset(stackData,0x33,sizeof(stackData));
    Integration_TimeMs=1000;AppIpc_Init();
    AppZbPersistence_Init(&zb);assert(!AppZbPersistence_HasSaved());assert(notify);
    AppZbPersistence_Process();assert(programs==0);
    AppZbPersistence_NetworkReady();assert(AppZbPersistence_HasSaved());
    unsigned count=programs;
    notify(&zb,NULL);assert(programs==count); /* callback no escribe Flash */
    Integration_TimeMs+=5000;AppZbPersistence_Process();assert(programs==count);
#if APP_IS_MASTER
    const uint64_t peer=UINT64_C(0x0080E1FFFE32FC18);
    assert(AppIpc_RegisterSlave(peer)==1);
    Integration_TimeMs+=5000;AppZbPersistence_Process();assert(programs>count);
    count=programs;
    /* Las temperaturas y su timestamp no se guardan en metadatos. */
    assert(AppIpc_SaveSlaveTemperature(peer,2424,true));
    Integration_TimeMs+=5000;AppZbPersistence_Process();assert(programs==count);
    memset(gSlaveStatus,0,sizeof(gSlaveStatus));
#endif
    AppZbPersistence_Init(&zb);assert(AppZbPersistence_HasSaved());
    struct ZbStartupT config={0};
    assert(AppZbPersistence_Restore(&config,Restored,&restores)==ZB_STATUS_SUCCESS && restores==1);
    AppZbPersistence_NetworkReady();
#if APP_IS_MASTER
    AppStatus status;assert(AppIpc_ReadSlaveStatus(1,&status));
    assert(status.device.ieee_address[7]==0x18 && status.updated_at_ms==0);
    assert((status.latest_sample.valid & APP_SAMPLE_VALID_TEMP)==0);
#endif
    count=programs;stackData[0]^=1;notify(&zb,NULL);Integration_TimeMs+=5000;
    failGet=true;AppZbPersistence_Process();assert(programs==count);
    failGet=false;Integration_TimeMs+=5000;AppZbPersistence_Process();assert(programs>count);
    assert(snapshots>0);Integration_FreeObjects();
    puts("PASS persistence + actual IPC: initial save, deferred callback, unchanged snapshot, registry-only updates, restoration, no old measurements, retry after failure");
    return 0;
}
