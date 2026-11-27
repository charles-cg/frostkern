#include "fake_rtos.h"
#include "queue.h"
#include "semphr.h"
#include "event_groups.h"
#include "task.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

uint32_t Integration_TimeMs;
bool Integration_MutexAllowed = true;
static void *objects[32];
static unsigned int objectCount;
static uint32_t notification;
typedef struct {unsigned int capacity,size,count;uint8_t bytes[];} TestQueue;
typedef struct {bool held;} TestMutex;
static void *Allocate(size_t size)
{
    assert(objectCount<32U);
    void *p=calloc(1U,size);assert(p!=NULL);objects[objectCount++]=p;return p;
}
void Integration_FreeObjects(void)
{for(unsigned int i=0;i<objectCount;++i)free(objects[i]);objectCount=0U;}
QueueHandle_t xQueueCreate(UBaseType_t capacity,UBaseType_t size)
{
    TestQueue *q=Allocate(sizeof(*q)+(size_t)capacity*size);
    q->capacity=capacity;q->size=size;return q;
}
BaseType_t xQueueOverwrite(QueueHandle_t handle,const void *item)
{
    TestQueue *q=handle;assert(q!=NULL && q->capacity==1U);
    memcpy(q->bytes,item,q->size);q->count=1U;return pdPASS;
}
BaseType_t xQueueSend(QueueHandle_t handle,const void *item,TickType_t wait)
{
    TestQueue *q=handle;assert(q!=NULL && wait==0U);
    if(q->count==q->capacity)return pdFALSE;
    memcpy(&q->bytes[q->count*q->size],item,q->size);++q->count;return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t handle,void *item,TickType_t wait)
{
    TestQueue *q=handle;(void)wait;assert(q!=NULL);
    if(q->count==0U)return pdFALSE;
    memcpy(item,q->bytes,q->size);--q->count;
    memmove(q->bytes,&q->bytes[q->size],q->count*q->size);return pdPASS;
}
BaseType_t xQueueSendFromISR(QueueHandle_t q,const void *item,BaseType_t *w)
{*w=pdFALSE;return xQueueSend(q,item,0U);}
BaseType_t xQueueReset(QueueHandle_t handle)
{TestQueue *q=handle;q->count=0U;return pdPASS;}
void vQueueDelete(QueueHandle_t handle){(void)handle;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){return Allocate(sizeof(TestMutex));}
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle,TickType_t wait)
{
    TestMutex *m=handle;(void)wait;assert(m!=NULL);
    if(!Integration_MutexAllowed || m->held)return pdFALSE;
    m->held=true;return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t handle)
{TestMutex *m=handle;assert(m!=NULL&&m->held);m->held=false;return pdTRUE;}
EventGroupHandle_t xEventGroupCreate(void){return Allocate(sizeof(EventBits_t));}
EventBits_t xEventGroupGetBits(EventGroupHandle_t h){return *(EventBits_t *)h;}
EventBits_t xEventGroupSetBits(EventGroupHandle_t h,EventBits_t bits)
{return *(EventBits_t *)h|=bits;}
TickType_t xTaskGetTickCount(void){return Integration_TimeMs/portTICK_PERIOD_MS;}
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,uint32_t depth,
    void *arg,UBaseType_t priority,TaskHandle_t *handle)
{
    (void)fn;(void)name;(void)arg;(void)priority;assert(depth>0U);
    if(handle!=NULL){*handle=(void *)1;}
    return pdPASS;
}
uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t wait)
{(void)wait;uint32_t n=notification;if(clear)notification=0U;return n;}
void vTaskNotifyGiveFromISR(TaskHandle_t task,BaseType_t *w)
{assert(task!=NULL);++notification;*w=pdFALSE;}
void Integration_Log(const char *format,...){(void)format;}

EventBits_t xEventGroupClearBits(EventGroupHandle_t h, EventBits_t bits)
{EventBits_t before=*(EventBits_t *)h;*(EventBits_t *)h &= ~bits;return before;}

__attribute__((weak)) void vTaskDelay(TickType_t ticks)
{Integration_TimeMs += ticks * portTICK_PERIOD_MS;}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t h, EventBits_t wanted,
                              BaseType_t clear, BaseType_t all, TickType_t wait)
{
    EventBits_t bits=*(EventBits_t *)h;
    bool ready=all ? (bits&wanted)==wanted : (bits&wanted)!=0U;
    if(!ready && wait!=0U) {vTaskDelay(wait);bits=*(EventBits_t *)h;}
    ready=all ? (bits&wanted)==wanted : (bits&wanted)!=0U;
    if(ready && clear) {*(EventBits_t *)h &= ~wanted;}
    return bits;
}
