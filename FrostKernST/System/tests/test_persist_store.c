#include "app_persist_store.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t flash[APP_PERSIST_SIZE];
static int failAt=-1,opCount;
static unsigned int erases;
static bool Read(void *ctx,uint32_t off,void *out,size_t n)
{(void)ctx;assert(off+n<=sizeof(flash));memcpy(out,flash+off,n);return true;}
static bool Erase(void *ctx,uint32_t page)
{
    (void)ctx;assert(page<3U);
    if(opCount++==failAt)return false;
    memset(flash+page*APP_PERSIST_PAGE_SIZE,0xFF,APP_PERSIST_PAGE_SIZE);++erases;return true;
}
static bool Program(void *ctx,uint32_t off,const uint8_t *data)
{
    (void)ctx;assert(off%16U==0 && off+16<=sizeof(flash));
    if(opCount++==failAt) {
        /* Simular una programacion interrumpida a mitad del quadword. */
        for(unsigned i=0;i<8;++i)flash[off+i]&=data[i];
        return false;
    }
    for(unsigned i=0;i<16;++i) {
        assert(flash[off+i]==0xFFU);flash[off+i]&=data[i];
    }
    return true;
}
static const AppPersistIo io={NULL,Read,Erase,Program};
static const AppPersistIdentity identity={UINT64_C(0x0080E1FFFE32FCE9),1,14,APP_PERSIST_FORMAT_ID};
static uint8_t first[113],second[APP_PERSIST_MAX_DATA],backup[APP_PERSIST_SIZE];
static const uint8_t peers[16]={0,0x80,0xE1,0xFF,0xFE,0x32,0xFC,0x18};
int main(void)
{
    AppPersistRecord record,loaded;
    memset(flash,0xFF,sizeof(flash));memset(first,0xA6,sizeof(first));
    memset(second,0x31,sizeof(second));
    assert(AppPersistStore_Crc32((const uint8_t *)"123456789",9)==UINT32_C(0xCBF43926));
    assert(!AppPersistStore_Load(&io,&identity,&loaded));
    assert(AppPersistStore_Save(&io,&identity,peers,first,sizeof(first),&record)==APP_PERSIST_SAVED);
    assert(record.valid && record.sequence==1 && record.length==sizeof(first));
    assert(memcmp(flash+record.offset+APP_PERSIST_DATA,first,sizeof(first))==0);
    assert(memcmp(record.peers,peers,16)==0);
    unsigned writes=(unsigned)opCount;
    assert(AppPersistStore_Save(&io,&identity,peers,first,sizeof(first),&record)==APP_PERSIST_UNCHANGED);
    assert(opCount==(int)writes);
    memcpy(backup,flash,sizeof(flash));
    /* Corte al anexar dentro de la misma pagina: tampoco perder la copia vieja. */
    for(int failure=0;failure<18;++failure) {
        memcpy(flash,backup,sizeof(flash));opCount=0;failAt=failure;
        uint8_t changed[113];memcpy(changed,first,sizeof(changed));changed[0]^=1;
        AppPersistResult result=AppPersistStore_Save(&io,&identity,peers,changed,sizeof(changed),&record);
        assert(AppPersistStore_Load(&io,&identity,&loaded));
        assert(loaded.sequence==(result==APP_PERSIST_ERROR?1U:2U));
    }
    /* Una nueva imagen grande requiere rotacion. Cortar en CADA operacion. */
    for(int failure=0;failure<520;++failure) {
        memcpy(flash,backup,sizeof(flash));opCount=0;failAt=failure;
        AppPersistResult result=AppPersistStore_Save(&io,&identity,peers,second,sizeof(second),&record);
        assert(AppPersistStore_Load(&io,&identity,&loaded));
        if(result==APP_PERSIST_ERROR) {
            assert(loaded.sequence==1 && loaded.length==sizeof(first));
            assert(memcmp(flash+loaded.offset+APP_PERSIST_DATA,first,sizeof(first))==0);
        } else {
            assert(result==APP_PERSIST_SAVED && loaded.sequence==2);
            assert(loaded.length==sizeof(second));
            break;
        }
    }
    failAt=-1;memcpy(flash,backup,sizeof(flash));opCount=0;
    /* Primera copia interrumpida no se acepta como una red guardada. */
    uint8_t empty[APP_PERSIST_SIZE];memset(empty,0xFF,sizeof(empty));
    for(int failure=0;failure<16;++failure) {
        memcpy(flash,empty,sizeof(flash));opCount=0;failAt=failure;
        AppPersistResult result=AppPersistStore_Save(&io,&identity,peers,first,sizeof(first),&record);
        assert(AppPersistStore_Load(&io,&identity,&loaded)==(result!=APP_PERSIST_ERROR));
    }
    failAt=-1;memcpy(flash,backup,sizeof(flash));opCount=0;
    /* Corrupcion de la copia nueva: recuperar la anterior. */
    assert(AppPersistStore_Save(&io,&identity,peers,second,sizeof(second),&record)==APP_PERSIST_SAVED);
    flash[record.offset+APP_PERSIST_DATA+17]^=1;
    assert(AppPersistStore_Load(&io,&identity,&loaded) && loaded.sequence==1);
    /* CRC de header, identidad, rol, canal y ABI de pila se validan. */
    AppPersistIdentity other=identity;other.ieee^=1;
    assert(!AppPersistStore_Load(&io,&other,&loaded));
    other=identity;other.role=2;assert(!AppPersistStore_Load(&io,&other,&loaded));
    other=identity;other.channel=15;assert(!AppPersistStore_Load(&io,&other,&loaded));
    other=identity;other.format^=1;assert(!AppPersistStore_Load(&io,&other,&loaded));
    /* Longitud corrupta con CRC de header valido no puede provocar lectura fuera de rango. */
    memcpy(flash,backup,sizeof(flash));
    memset(flash+12,0xFF,4);memset(flash+20,0,4);
    uint32_t headerCrc=AppPersistStore_Crc32(flash,80);
    for(unsigned i=0;i<4;++i)flash[20+i]=(uint8_t)(headerCrc>>(i*8));
    assert(!AppPersistStore_Load(&io,&identity,&loaded));
    assert(AppPersistStore_Save(&io,&identity,peers,first,0,&record)==APP_PERSIST_ERROR);
    assert(AppPersistStore_Save(&io,&identity,peers,first,APP_PERSIST_MAX_DATA+1U,&record)==APP_PERSIST_ERROR);
    /* Muchos registros pequenos comparten pagina antes de borrarla. */
    memset(flash,0xFF,sizeof(flash));opCount=0;erases=0;
    for(unsigned i=0;i<200;++i) {
        first[0]=(uint8_t)i;
        assert(AppPersistStore_Save(&io,&identity,peers,first,sizeof(first),&record)==APP_PERSIST_SAVED);
    }
    assert(record.sequence==200U && erases<10U);
    puts("PASS journal: CRC, all interrupted writes, rotation, append wear leveling, corruption fallback, role/channel/EUI/ABI isolation, bounds, unchanged data");
    return 0;
}
