#ifndef FROSTKERN_APP_PERSIST_STORE_H
#define FROSTKERN_APP_PERSIST_STORE_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* STM32WBA55CG: ultimas tres paginas de 8 KiB, reservadas por el linker. */
#define APP_PERSIST_BASE       UINT32_C(0x080FA000)
#define APP_PERSIST_PAGE_SIZE  8192U
#define APP_PERSIST_PAGE_COUNT 3U
#define APP_PERSIST_SIZE       (APP_PERSIST_PAGE_SIZE * APP_PERSIST_PAGE_COUNT)
#define APP_PERSIST_HEADER     80U
#define APP_PERSIST_COMMIT     16U
#define APP_PERSIST_DATA       (APP_PERSIST_HEADER + APP_PERSIST_COMMIT)
#define APP_PERSIST_MAX_DATA   (APP_PERSIST_PAGE_SIZE - APP_PERSIST_DATA)
#define APP_PERSIST_FORMAT_ID  UINT32_C(0x010A0023) /* WBA 1.10.0 / Zigbee R23 */

typedef struct {
    uint64_t ieee;
    uint8_t role;
    uint8_t channel;
    uint32_t format;
} AppPersistIdentity;

typedef struct {
    void *context;
    bool (*read)(void *, uint32_t offset, void *, size_t);
    bool (*erase)(void *, uint32_t page);
    bool (*program)(void *, uint32_t offset, const uint8_t quad[16]);
} AppPersistIo;

typedef struct {
    bool valid;
    uint32_t offset;
    uint32_t sequence;
    uint32_t length;
    uint8_t peers[16]; /* IEEE de Slave 1 y 2; nunca muestras ni tiempos. */
} AppPersistRecord;

typedef enum { APP_PERSIST_ERROR, APP_PERSIST_UNCHANGED, APP_PERSIST_SAVED } AppPersistResult;

bool AppPersistStore_Load(const AppPersistIo *, const AppPersistIdentity *, AppPersistRecord *);
AppPersistResult AppPersistStore_Save(const AppPersistIo *, const AppPersistIdentity *,
    const uint8_t peers[16], const uint8_t *data, uint32_t length, AppPersistRecord *);
uint32_t AppPersistStore_Crc32(const uint8_t *, size_t);
#endif
