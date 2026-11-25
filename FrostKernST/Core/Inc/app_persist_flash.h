#ifndef FROSTKERN_APP_PERSIST_FLASH_H
#define FROSTKERN_APP_PERSIST_FLASH_H
#include "app_persist_store.h"
bool AppPersistFlash_Open(AppPersistIo *);
bool AppPersistFlash_Begin(void);
void AppPersistFlash_End(void);
const uint8_t *AppPersistFlash_Data(uint32_t offset);
#endif
