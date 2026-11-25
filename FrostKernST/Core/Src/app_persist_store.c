#include "app_persist_store.h"
#include <string.h>

#define MAGIC UINT32_C(0x465A4250)
static const uint8_t commit[16] = {
    0x46,0x4B,0x2D,0x43,0x4F,0x4D,0x4D,0x49,0x54,0x2D,0x56,0x31,0xA5,0x5A,0xC3,0x3C
};
static uint32_t Get32(const uint8_t *p)
{ return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static void Put32(uint8_t *p,uint32_t v)
{ for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(8*i)); }
static void Put64(uint8_t *p,uint64_t v)
{ for(unsigned i=0;i<8;++i)p[i]=(uint8_t)(v>>(56-8*i)); }
static uint32_t CrcStep(uint32_t crc,const uint8_t *p,size_t n)
{
    while(n--) {
        crc^=*p++;
        for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1U)?UINT32_C(0xEDB88320):0U);
    }
    return crc;
}
uint32_t AppPersistStore_Crc32(const uint8_t *p,size_t n)
{ return ~CrcStep(UINT32_MAX,p,n); }
static uint32_t Total(uint32_t n)
{ return APP_PERSIST_DATA+((n+15U)&~15U); }
static bool Read(const AppPersistIo *io,uint32_t off,void *p,size_t n)
{ return off<=APP_PERSIST_SIZE && n<=APP_PERSIST_SIZE-off && io->read(io->context,off,p,n); }
static bool Equal(const AppPersistIo *io,uint32_t off,const uint8_t *p,size_t n)
{
    uint8_t chunk[16];
    while(n) {
        size_t count=n<sizeof(chunk)?n:sizeof(chunk);
        if(!Read(io,off,chunk,count)||memcmp(chunk,p,count)!=0)return false;
        off+=(uint32_t)count;p+=count;n-=count;
    }
    return true;
}
static bool Erased(const AppPersistIo *io,uint32_t off,size_t n)
{
    uint8_t chunk[16];
    while(n) {
        size_t count=n<sizeof(chunk)?n:sizeof(chunk);
        if(!Read(io,off,chunk,count))return false;
        for(size_t i=0;i<count;++i)if(chunk[i]!=0xFFU)return false;
        off+=(uint32_t)count;n-=count;
    }
    return true;
}
static bool DataCrc(const AppPersistIo *io,uint32_t off,uint32_t n,uint32_t expected)
{
    uint8_t b[32];uint32_t crc=UINT32_MAX;
    while(n) {
        uint32_t count=n<sizeof(b)?n:(uint32_t)sizeof(b);
        if(!Read(io,off,b,count))return false;
        crc=CrcStep(crc,b,count);off+=count;n-=count;
    }
    return ~crc==expected;
}
static bool Header(const AppPersistIo *io,uint32_t off,uint32_t remaining,uint8_t h[80])
{
    if(!Read(io,off,h,80)||Get32(h)!=MAGIC||Get32(h+4)!=1U)return false;
    uint32_t crc=Get32(h+20);Put32(h+20,0);
    bool valid=AppPersistStore_Crc32(h,80)==crc;
    Put32(h+20,crc);
    uint32_t n=Get32(h+12);
    return valid && n>0U && n<=APP_PERSIST_MAX_DATA && Total(n)<=remaining;
}
bool AppPersistStore_Load(const AppPersistIo *io,const AppPersistIdentity *id,AppPersistRecord *out)
{
    memset(out,0,sizeof(*out));uint8_t own[8];Put64(own,id->ieee);
    for(uint32_t page=0;page<APP_PERSIST_PAGE_COUNT;++page) {
        uint32_t begin=page*APP_PERSIST_PAGE_SIZE,end=begin+APP_PERSIST_PAGE_SIZE;
        for(uint32_t off=begin;off+APP_PERSIST_DATA<=end;) {
            uint8_t h[80];
            if(!Header(io,off,end-off,h))break; /* cola vacia o incompleta */
            uint32_t n=Get32(h+12),seq=Get32(h+8);
            bool newer=!out->valid || (uint32_t)(seq-out->sequence)<UINT32_C(0x80000000);
            if(newer && h[24]==id->role && h[25]==id->channel &&
               Get32(h+52)==id->format && memcmp(h+32,own,8)==0 &&
               Equal(io,off+80,commit,sizeof(commit)) &&
               DataCrc(io,off+APP_PERSIST_DATA,n,Get32(h+16))) {
                out->valid=true;out->offset=off;out->sequence=seq;out->length=n;
                /* Dos IEEE se serializan en offsets separados de la identidad. */
                memcpy(out->peers,h+56,16);
            }
            off+=Total(n);
        }
    }
    return out->valid;
}
AppPersistResult AppPersistStore_Save(const AppPersistIo *io,const AppPersistIdentity *id,
    const uint8_t peers[16],const uint8_t *data,uint32_t n,AppPersistRecord *latest)
{
    if(data==NULL||n==0U||n>APP_PERSIST_MAX_DATA)return APP_PERSIST_ERROR;
    AppPersistRecord previous;
    AppPersistStore_Load(io,id,&previous);
    if(previous.valid && previous.length==n && memcmp(previous.peers,peers,16)==0 &&
       Equal(io,previous.offset+APP_PERSIST_DATA,data,n)) {
        *latest=previous;return APP_PERSIST_UNCHANGED;
    }
    uint32_t page=previous.valid?previous.offset/APP_PERSIST_PAGE_SIZE:0U;
    uint32_t off=previous.valid?previous.offset+Total(previous.length):0U;
    uint32_t end=(page+1U)*APP_PERSIST_PAGE_SIZE;
    if(!previous.valid || Total(n)>end-off || !Erased(io,off,Total(n))) {
        if(previous.valid)page=(page+1U)%APP_PERSIST_PAGE_COUNT;
        off=page*APP_PERSIST_PAGE_SIZE;
        if(!io->erase(io->context,page))return APP_PERSIST_ERROR;
    }
    uint8_t h[80]={0};uint32_t seq=previous.valid?previous.sequence+1U:1U;
    Put32(h,MAGIC);Put32(h+4,1U);Put32(h+8,seq);Put32(h+12,n);
    Put32(h+16,AppPersistStore_Crc32(data,n));h[24]=id->role;h[25]=id->channel;
    Put64(h+32,id->ieee);Put32(h+52,id->format);memcpy(h+56,peers,16);
    Put32(h+20,AppPersistStore_Crc32(h,sizeof(h)));
    for(uint32_t i=0;i<80;i+=16)
        if(!io->program(io->context,off+i,h+i))return APP_PERSIST_ERROR;
    for(uint32_t i=0;i<n;i+=16) {
        uint8_t block[16];memset(block,0xFF,sizeof(block));
        uint32_t count=n-i<16U?n-i:16U;memcpy(block,data+i,count);
        if(!io->program(io->context,off+APP_PERSIST_DATA+i,block))return APP_PERSIST_ERROR;
    }
    if(!DataCrc(io,off+APP_PERSIST_DATA,n,Get32(h+16)))return APP_PERSIST_ERROR;
    /* Commit se programa al final y en una unidad Flash distinta del header. */
    if(!io->program(io->context,off+80,commit))return APP_PERSIST_ERROR;
    if(!AppPersistStore_Load(io,id,latest)||latest->offset!=off)return APP_PERSIST_ERROR;
    return APP_PERSIST_SAVED;
}
