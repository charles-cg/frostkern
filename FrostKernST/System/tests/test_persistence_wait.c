#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#define osWaitForever UINT32_MAX
static int stZbCallbackTimeOutTimer,EndWaitCallbackSemaphore;
static bool armed;
static bool alreadyCompleted;
static void UTIL_TIMER_StartWithPeriod(void *timer,uint32_t duration)
{assert(timer==&stZbCallbackTimeOutTimer && duration==60000U);armed=true;}
static void UTIL_TIMER_Stop(void *timer)
{assert(timer==&stZbCallbackTimeOutTimer);armed=false;}
static void osSemaphoreAcquire(int semaphore,uint32_t wait)
{
    assert(semaphore==EndWaitCallbackSemaphore && wait==osWaitForever);
    /* Caso asincrono: el callback cancela timer y desbloquea la espera. */
    if(!alreadyCompleted)UTIL_TIMER_Stop(&stZbCallbackTimeOutTimer);
}
#include "persistence_wait.inc"
int main(void)
{
    alreadyCompleted=true;APP_ZIGBEE_WaitForCallback(60000U);assert(!armed);
    alreadyCompleted=false;APP_ZIGBEE_WaitForCallback(60000U);assert(!armed);
    puts("PASS real callback wait: no late timeout after synchronous or asynchronous restore");
    return 0;
}
