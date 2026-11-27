#include "app_role.h"
#if APP_IS_MASTER
#include "app_master_control.h"
#include "app_fan_pwm.h"
#include "app_uart_protocol.h"
#include "app_ipc.h"
#include "app_task_config.h"
#include "ds18b20.h"
#include "main.h"
#include "task.h"
#include "log_module.h"
#include <string.h>
typedef struct {
    uint8_t type, enabled, percent, bench_mode;
    AppFanConfig config;
} MasterCommand;
static QueueHandle_t commands;
static SemaphoreHandle_t lock;
static AppMasterSnapshot state;
static uint32_t benchAt;
static uint8_t benchPercent;
static uint32_t Now(void) { return (uint32_t)xTaskGetTickCount()*portTICK_PERIOD_MS; }
static uint64_t ReadIeee(const uint8_t *p)
{ uint64_t v=0U; for(unsigned i=0U;i<8U;++i){v=(v<<8U)|p[i];} return v; }
static int16_t Read16(const uint8_t *p)
{ return (int16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8U)); }
uint8_t AppMaster_QueueCommand(uint8_t type, const uint8_t *p, uint8_t length)
{
    MasterCommand c={0}; c.type=type;
    if(p==NULL) { return APP_UART_ERR_LENGTH; }
    if(type==APP_UART_SET_AC_FAN) {
        if(length!=2U) { return APP_UART_ERR_LENGTH; }
        if(p[0]>1U || p[1]>100U) { return APP_UART_ERR_VALUE; }
        c.enabled=p[0]; c.percent=p[1];
    } else if(type==APP_UART_SET_AUTO_FAN) {
        if(length!=16U) { return APP_UART_ERR_LENGTH; }
        if(p[0]>1U) { return APP_UART_ERR_VALUE; }
        c.config.enabled=p[0]!=0U; c.config.target_ieee=ReadIeee(p+1);
        c.config.off_centi=Read16(p+9); c.config.on_centi=Read16(p+11);
        c.config.full_centi=Read16(p+13); c.config.minimum_percent=p[15];
        if(c.config.enabled && !AppFanConfig_Valid(&c.config)) { return APP_UART_ERR_VALUE; }
    } else if(type==APP_UART_FAN_BENCH) {
        if(length!=2U) { return APP_UART_ERR_LENGTH; }
        /* ENB bench only, 5 s lease. ENA already has manual control. */
        if(p[0]>1U || p[1]>100U) { return APP_UART_ERR_VALUE; }
        c.bench_mode=p[0]; c.percent=p[1];
    } else { return APP_UART_ERR_UNSUPPORTED; }
    if(commands==NULL) { return APP_UART_ERR_NOT_READY; }
    return xQueueSend(commands,&c,0U)==pdPASS ? APP_UART_OK_QUEUED : APP_UART_ERR_QUEUE_FULL;
}
bool AppMaster_Read(AppMasterSnapshot *out)
{
    if(out==NULL || lock==NULL || xSemaphoreTake(lock,pdMS_TO_TICKS(20U))!=pdTRUE) { return false; }
    *out=state; xSemaphoreGive(lock); return true;
}
static void AirTemperatureTask(void *unused)
{
    (void)unused; DS18B20_Init();
    /* Independent FreeRTOS tick reference, with interrupts enabled throughout.
     * Approximate check only: a short capture may vary around tick boundaries. */
    DS18B20_Diagnostics before, after;
    DS18B20_GetDiagnostics(&before);
    uint32_t tickBefore=Now();
    vTaskDelay(pdMS_TO_TICKS(20U));
    DS18B20_GetDiagnostics(&after);
    uint32_t elapsed=Now()-tickBefore;
    uint32_t counterDelta=after.counter-before.counter;
    LOG_INFO_APP("[DS-FIX2] DQ release before each reset; input probe enabled");
    LOG_INFO_APP("[DS-FIX] Joaquin TIM2 timebase restored; PSC=%u",(unsigned)after.prescaler);
    LOG_INFO_APP("[DS-DIAG] RCC_PCLK1=%lu timer_ticks=%lu elapsed_ms=%lu ticks_per_ms=%lu (expected about 1000)",
        (unsigned long)after.pclk1_hz,(unsigned long)counterDelta,(unsigned long)elapsed,
        (unsigned long)(elapsed!=0U ? counterDelta/elapsed : 0U));
    for(;;) {
        bool started=DS18B20_StartConversion();
        vTaskDelay(pdMS_TO_TICKS(1000U)); /* >=750 ms at 12 bit, does not busy wait. */
        int16_t t=0;
        DS18B20_Status result=started ? DS18B20_ReadTemperature(&t) : DS18B20_NO_PRESENCE;
        if(xSemaphoreTake(lock,pdMS_TO_TICKS(20U))==pdTRUE) {
            state.air_seen=true; state.air_valid=result==DS18B20_OK;
            state.air_centi=state.air_valid ? t : 0; state.air_updated_ms=Now();
            xSemaphoreGive(lock);
        }
        if(result==DS18B20_OK) { LOG_INFO_APP("DS18B20 aire: %d centi-C",(int)t); }
        else {
            DS18B20_Diagnostics d; DS18B20_GetDiagnostics(&d);
            LOG_WARNING_APP("DS18B20: lectura no disponible, status=%u",(unsigned)result);
            LOG_WARNING_APP("[DS-DIAG] fault=%u idle=%u presence=%u released=%u PB1_mode=%u OD=%u PSC=%u",
                (unsigned)d.fault,(unsigned)d.idle_high,(unsigned)d.presence_low,(unsigned)d.final_high,
                (unsigned)d.mode,(unsigned)d.open_drain,(unsigned)d.prescaler);
            LOG_WARNING_APP("[DS-LINE] ODR_before=%u ODR_released=%u INPUT_high=%u",
                (unsigned)d.odr_before,(unsigned)d.odr_after,(unsigned)d.input_high);
        }
    }
}
/* Called solely by MasterControl. Source is IEEE, NEVER first registered slot. */
static void FindInput(AppMasterSnapshot *s, uint32_t now, bool *valid)
{
    *valid=false; s->input_age_ms=UINT32_MAX; s->input_centi=0;
    for(uint8_t id=1U;id<=APP_SLAVE_COUNT;++id) {
        AppStatus peer;
        if(!AppIpc_ReadSlaveStatus(id,&peer) ||
            ReadIeee(peer.device.ieee_address)!=s->automatic.target_ieee) { continue; }
        if((peer.reports_seen&APP_SAMPLE_VALID_TEMP)!=0U) {
            s->input_age_ms=now-peer.temperature_report_at_ms;
            s->input_centi=peer.latest_sample.temperature_centi_c;
            *valid=(peer.latest_sample.valid&APP_SAMPLE_VALID_TEMP)!=0U && AppIpc_NetworkStarted();
        }
        break;
    }
}
static void Step(uint32_t now)
{
    MasterCommand c;
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(20U))!=pdTRUE) { return; }
    while(xQueueReceive(commands,&c,0U)==pdPASS) {
        if(c.type==APP_UART_SET_AC_FAN) { state.ac_enabled=c.enabled!=0U; state.ac_requested=c.percent; }
        else if(c.type==APP_UART_SET_AUTO_FAN) {
            state.automatic=c.config; state.bench_mode=0U; state.radiator.running=false;
        } else if(c.type==APP_UART_FAN_BENCH) {
            /* An explicit bench request disables AUTO, which must be re-armed. */
            state.automatic.enabled=false; state.bench_mode=c.bench_mode;
            benchAt=now; benchPercent=c.percent;
        }
    }
    AppMasterSnapshot next=state;
    /* Never acquire IPC mutex while holding this state mutex. */
    xSemaphoreGive(lock);
    bool valid; FindInput(&next,now,&valid);
    if(next.bench_mode!=0U && (uint32_t)(now-benchAt)>=APP_FAN_BENCH_LEASE_MS) { next.bench_mode=0U; }
    next.radiator=AppFan_Evaluate(&next.automatic,next.radiator.running,valid,next.input_centi,next.input_age_ms);
    if(next.bench_mode!=0U) {
        next.radiator=(AppFanResult){benchPercent!=0U,benchPercent,APP_FAN_BENCH};
    }
    next.ac_applied=next.ac_enabled ? next.ac_requested : 0U;
    AppFanPwm_Set(next.ac_applied,next.radiator.duty);
    if(xSemaphoreTake(lock,pdMS_TO_TICKS(20U))==pdTRUE) {
        /* Air task owns these four fields. Do not overwrite a concurrent conversion. */
        next.air_seen=state.air_seen; next.air_valid=state.air_valid;
        next.air_centi=state.air_centi; next.air_updated_ms=state.air_updated_ms;
        state=next; xSemaphoreGive(lock);
    }
}
static void ControlTask(void *unused)
{ (void)unused; AppFanPwm_Init(); for(;;){Step(Now());vTaskDelay(pdMS_TO_TICKS(100U));} }
void AppMaster_Start(void)
{
    if(commands!=NULL) { return; }
    lock=xSemaphoreCreateMutex(); commands=xQueueCreate(8U,sizeof(MasterCommand));
    if(lock==NULL || commands==NULL) { Error_Handler(); return; }
    memset(&state,0,sizeof(state)); state.automatic.target_ieee=APP_FAN_DEFAULT_TARGET_IEEE;
    state.input_age_ms=UINT32_MAX;
    if(xTaskCreate(ControlTask,"MasterControl",APP_STACK_DEPTH(APP_TASK_STACK_CONTROL),NULL,
        APP_TASK_PRIO_SUPERVISOR,NULL)!=pdPASS ||
       xTaskCreate(AirTemperatureTask,"AirTemperature",APP_STACK_DEPTH(APP_TASK_STACK_SENSOR),NULL,
        APP_TASK_PRIO_SUPERVISOR,NULL)!=pdPASS) { Error_Handler(); }
}
#endif
