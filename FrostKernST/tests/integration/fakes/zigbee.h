#ifndef TEST_ZIGBEE_H
#define TEST_ZIGBEE_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
struct ZigBeeT {int unused;};
struct ZbStartupT {int unused;};
enum ZbStatusCodeT {ZB_STATUS_SUCCESS=0,ZB_ZDP_STATUS_TIMEOUT=0x85};
uint64_t ZbExtendedAddress(struct ZigBeeT *);
bool ZbPersistNotifyRegister(struct ZigBeeT *,void (*)(struct ZigBeeT *,void *),void *);
unsigned int ZbPersistGet(struct ZigBeeT *,uint8_t *,unsigned int);
enum ZbStatusCodeT ZbStartupPersist(struct ZigBeeT *,const void *,unsigned int,
    struct ZbStartupT *,void (*)(enum ZbStatusCodeT,void *),void *);
#endif
