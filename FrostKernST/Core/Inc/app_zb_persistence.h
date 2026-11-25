#ifndef FROSTKERN_APP_ZB_PERSISTENCE_H
#define FROSTKERN_APP_ZB_PERSISTENCE_H
#include "zigbee.h"

/* Cambiar a 1 solo para borrar la red guardada; volver a 0 despues. */
#ifndef APP_ZB_PERSIST_ERASE_ON_BOOT
#define APP_ZB_PERSIST_ERASE_ON_BOOT 0
#endif
void AppZbPersistence_Init(struct ZigBeeT *);
bool AppZbPersistence_HasSaved(void);
enum ZbStatusCodeT AppZbPersistence_Restore(struct ZbStartupT *,
    void (*callback)(enum ZbStatusCodeT,void *),void *);
void AppZbPersistence_Enable(void);
void AppZbPersistence_NetworkReady(void);
void AppZbPersistence_Process(void);
#endif
