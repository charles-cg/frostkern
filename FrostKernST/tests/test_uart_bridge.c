/* Tests the production bridge against fake HAL/RTOS services, not on hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../Core/Src/app_uart_bridge.c"

UART_HandleTypeDef huart2;
static uint8_t queueMarker;
QueueHandle_t qZbCommand = &queueMarker;
static AppStatus fakeStatus[APP_SLAVE_COUNT];
static bool networkReady, commandQueued = true, txCompleteEnabled = true;
static bool txBusy;
static uint8_t requestedSeconds, requestedSequence;
static uint32_t fakeTicks = 100U, notification;
static unsigned int capturedCount, abortCount, createCount;
static AppUartFrame captured[32];

QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t size)
{ assert(length == 256U && size == 1U); return &queueMarker; }
BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *p, BaseType_t *w)
{ (void)q; (void)p; *w = pdFALSE; return pdPASS; }
BaseType_t xQueueReceive(QueueHandle_t q, void *p, TickType_t t)
{ (void)q; (void)p; (void)t; return pdFALSE; }
BaseType_t xQueueReset(QueueHandle_t q) { (void)q; return pdPASS; }
void vQueueDelete(QueueHandle_t q) { (void)q; }
BaseType_t xTaskCreate(void (*fn)(void *), const char *name, uint32_t depth,
    void *arg, UBaseType_t priority, TaskHandle_t *handle)
{
    assert(fn == BridgeTask && strcmp(name, "BridgeTask") == 0);
    assert(depth == 384U && priority == 20U && arg == NULL);
    *handle = &queueMarker; ++createCount; return pdPASS;
}
TickType_t xTaskGetTickCount(void) { return fakeTicks; }
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks)
{ (void)ticks; uint32_t old = notification; if (clear) notification = 0; return old; }
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *w)
{ assert(task == bridgeTask); ++notification; *w = pdFALSE; }
void vTaskDelete(TaskHandle_t task) { (void)task; assert(0); }
void MX_USART2_UART_Init(void) {}
void Error_Handler(void) { assert(0); }
void HAL_NVIC_DisableIRQ(int irq) { assert(irq == USART2_IRQn); }
void HAL_NVIC_EnableIRQ(int irq) { assert(irq == USART2_IRQn); }
void HAL_NVIC_SetPriority(int irq, uint32_t priority, uint32_t sub)
{ assert(irq == USART2_IRQn && priority == 8U && sub == 0U); }
void HAL_GPIO_WritePin(void *port, uint16_t pin, int state)
{ (void)port; (void)pin; (void)state; }
HAL_StatusTypeDef HAL_UART_RegisterCallback(UART_HandleTypeDef *u,
    HAL_UART_CallbackIDTypeDef id, void (*callback)(UART_HandleTypeDef *))
{ assert(u == &huart2 && callback != NULL); (void)id; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *u, uint8_t *p, uint16_t n)
{ assert(u == &huart2 && p != NULL && n == 1U); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *u, uint8_t *p, uint16_t n)
{
    assert(u == &huart2);
    if (txBusy) return HAL_BUSY;
    AppUartParser decoder; AppUartParser_Init(&decoder);
    for (uint16_t i = 0; i < n; ++i) AppUartParser_Push(&decoder, p[i]);
    assert(capturedCount < 32U);
    assert(AppUartParser_Next(&decoder, &captured[capturedCount++]));
    if (txCompleteEnabled) TxComplete(u);
    return HAL_OK;
}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *u)
{ assert(u == &huart2); return HAL_OK; }
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *u)
{ assert(u == &huart2); ++abortCount; return HAL_OK; }
bool AppIpc_NetworkStarted(void) { return networkReady; }
bool AppIpc_RequestPermitJoin(uint8_t seconds, uint8_t sequence)
{ requestedSeconds = seconds; requestedSequence = sequence; return commandQueued; }
bool AppIpc_ReadSlaveStatus(uint8_t id, AppStatus *out)
{ assert(id >= 1U && id <= APP_SLAVE_COUNT); *out = fakeStatus[id - 1U]; return true; }

static uint32_t Read32(const uint8_t *p)
{ return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static AppUartFrame *Last(void) { assert(capturedCount); return &captured[capturedCount - 1U]; }

int main(void)
{
    AppUartBridge_Start(); AppUartBridge_Start();
    assert(createCount == 1U);
    AppUartFrame request = {.type = APP_UART_PING, .sequence = 42U};
    HandleFrame(&request);
    assert(Last()->type == APP_UART_PONG && Last()->sequence == 42U);
    assert(Last()->length == 6U && Read32(&Last()->payload[2]) == 1000U);
    request.length = 1U;
    HandleFrame(&request); assert(Last()->payload[1] == APP_UART_ERR_LENGTH);
    request.type = APP_UART_PERMIT_JOIN; request.payload[0] = 180U;
    HandleFrame(&request); assert(Last()->payload[1] == APP_UART_ERR_NOT_READY);
    networkReady = true; commandQueued = false;
    HandleFrame(&request); assert(Last()->payload[1] == APP_UART_ERR_QUEUE_FULL);
    commandQueued = true;
    HandleFrame(&request);
    assert(Last()->type == APP_UART_ACK && Last()->payload[1] == APP_UART_OK_QUEUED);
    assert(requestedSeconds == 180U && requestedSequence == 42U);
    request.payload[0] = 255U;
    HandleFrame(&request); assert(Last()->payload[1] == APP_UART_ERR_VALUE);
    request.type = APP_UART_SET_SETPOINT;
    HandleFrame(&request); assert(Last()->payload[1] == APP_UART_ERR_UNSUPPORTED);
    unsigned int before = capturedCount;
    request.type = APP_UART_ACK; HandleFrame(&request); assert(capturedCount == before);

    PublishSlave(1U); assert(capturedCount == before); /* Unknown IEEE: no fake reading. */
    fakeStatus[0].device.ieee_address[7] = 9U;
    fakeStatus[0].updated_at_ms = 500U;
    fakeStatus[0].latest_sample.valid = APP_SAMPLE_VALID_TEMP;
    fakeStatus[0].latest_sample.temperature_centi_c = -1250;
    PublishSlave(1U);
    assert(capturedCount == before + 2U); /* DEVICE_JOINED then telemetry. */
    assert(captured[before].type == APP_UART_DEVICE_JOINED);
    assert(Last()->type == APP_UART_TELEMETRY_TEMP && Last()->length == 21U);
    assert(Last()->payload[10] == (APP_UART_TEMP_VALID | APP_UART_TEMP_REPORT_SEEN));
    assert((uint16_t)(Last()->payload[11] | (uint16_t)Last()->payload[12] << 8) == (uint16_t)-1250);
    assert(Read32(&Last()->payload[17]) == 500U);
    before = capturedCount; PublishSlave(1U); assert(capturedCount == before + 1U);
    fakeTicks = 18100U; PublishSlave(1U);
    assert(Last()->payload[10] == (APP_UART_TEMP_STALE | APP_UART_TEMP_REPORT_SEEN));
    assert(Last()->payload[11] == 0U && Last()->payload[12] == 0U);
    fakeTicks = 100U; fakeStatus[0].latest_sample.valid = 0;
    PublishSlave(1U); assert(Last()->payload[10] == APP_UART_TEMP_REPORT_SEEN);
    fakeStatus[0].updated_at_ms = 0;
    PublishSlave(1U); assert(Last()->payload[10] == 0U);
    assert(Read32(&Last()->payload[17]) == UINT32_MAX);
    fakeStatus[0].updated_at_ms = UINT32_MAX - 99U;
    fakeStatus[0].latest_sample.valid = APP_SAMPLE_VALID_TEMP;
    PublishSlave(1U); assert(Read32(&Last()->payload[17]) == 1100U);

    txCompleteEnabled = false; before = txErrors;
    assert(!SendFrame(APP_UART_PING, 1U, NULL, 0U));
    assert(txErrors == before + 1U && abortCount == 1U);
    txCompleteEnabled = true; txBusy = true; before = txErrors;
    assert(!SendFrame(APP_UART_PING, 1U, NULL, 0U)); assert(txErrors == before + 1U);
    txBusy = false;
    UartError(&huart2); assert(rxRestart && uartErrors == 1U);
    RestartReception(); assert(!rxRestart);
    puts("UART bridge: task creation, ping, validation, queued ACK, unsupported commands, telemetry validity/staleness/wrap, TX failures and RX recovery passed (fake HAL/RTOS).");
    return 0;
}
