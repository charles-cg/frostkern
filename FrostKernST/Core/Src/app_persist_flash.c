/* Flash HAL siguiendo la geometria y el acceso del ejemplo Persist de ST WBA 1.10.0. */
#include "stm32wbaxx_hal.h"
#include "app_persist_flash.h"
#include "stm32wbaxx_ll_icache.h"
#include <string.h>

extern uint8_t __zb_nvm_start__,__zb_nvm_end__;
static bool opened;
static bool cacheWasEnabled;
static bool busy;

static bool Read(void *ctx,uint32_t off,void *out,size_t n)
{
    (void)ctx;
    if(!opened||off>APP_PERSIST_SIZE||n>APP_PERSIST_SIZE-off)return false;
    memcpy(out,(const void *)(uintptr_t)(APP_PERSIST_BASE+off),n);return true;
}
static bool Erase(void *ctx,uint32_t page)
{
    (void)ctx;if(!busy||page>=APP_PERSIST_PAGE_COUNT)return false;
    FLASH_EraseInitTypeDef erase={0};uint32_t error=0;
    erase.TypeErase=FLASH_TYPEERASE_PAGES;
    erase.Page=(APP_PERSIST_BASE-FLASH_BASE)/FLASH_PAGE_SIZE+page;
    erase.NbPages=1;
    return HAL_FLASHEx_Erase(&erase,&error)==HAL_OK;
}
static bool Program(void *ctx,uint32_t off,const uint8_t data[16])
{
    (void)ctx;
    if(!busy||off%16U!=0U||off>APP_PERSIST_SIZE-16U)return false;
    uint32_t aligned[4];memcpy(aligned,data,sizeof(aligned));
    uint32_t address=APP_PERSIST_BASE+off;
    return HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD,address,
               (uint32_t)(uintptr_t)aligned)==HAL_OK &&
           memcmp((const void *)(uintptr_t)address,data,16)==0;
}
bool AppPersistFlash_Open(AppPersistIo *io)
{
    opened=(uintptr_t)&__zb_nvm_start__==APP_PERSIST_BASE &&
        (uintptr_t)&__zb_nvm_end__==APP_PERSIST_BASE+APP_PERSIST_SIZE &&
        FLASH_PAGE_SIZE==APP_PERSIST_PAGE_SIZE && FLASH_SIZE==UINT32_C(0x100000);
    if(opened)*io=(AppPersistIo){NULL,Read,Erase,Program};
    return opened;
}
bool AppPersistFlash_Begin(void)
{
    if(!opened||busy)return false;
    busy=true;cacheWasEnabled=LL_ICACHE_IsEnabled()!=0U;
    if(cacheWasEnabled)LL_ICACHE_Disable();
    if(HAL_FLASH_Unlock()!=HAL_OK){AppPersistFlash_End();return false;}
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_ALL_ERRORS);
    return true;
}
void AppPersistFlash_End(void)
{
    if(!busy)return;
    (void)HAL_FLASH_Lock();
    if(cacheWasEnabled)LL_ICACHE_Enable();
    busy=false;
}
const uint8_t *AppPersistFlash_Data(uint32_t offset)
{
    if(!opened||offset>=APP_PERSIST_SIZE)return NULL;
    return (const uint8_t *)(uintptr_t)(APP_PERSIST_BASE+offset);
}
