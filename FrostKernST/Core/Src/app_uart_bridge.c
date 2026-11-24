#include "app_uart_bridge.h"
#include "app_role.h"

#if APP_IS_MASTER

#include "app_conf.h"
#include "app_ipc.h"
#include "app_task_config.h"
#include "app_uart_protocol.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <string.h>

#if (CFG_LPM_LEVEL != 0)
#error "UART bridge v1 requires CFG_LPM_LEVEL=0; Stop-mode UART wakeup is not implemented."
#endif
#if (USE_HAL_UART_REGISTER_CALLBACKS != 1)
#error "UART bridge v1 requires USE_HAL_UART_REGISTER_CALLBACKS=1."
#endif

#define RX_QUEUE_LENGTH 256U
#define PUBLICATION_MS 1000U
#define RX_FRAME_GAP_MS 100U
#define TEMP_STALE_MS 180000U
#define TX_TIMEOUT_MS 100U

extern UART_HandleTypeDef huart2;

static QueueHandle_t rxQueue;
static TaskHandle_t bridgeTask;
static uint8_t rxByte;
static uint8_t txBuffer[APP_UART_MAX_FRAME];
static volatile bool rxRestart;
static volatile uint32_t rxDropped;
static volatile uint32_t uartErrors;
static uint32_t txErrors;
static uint8_t txSequence;
static uint8_t announcedIeee[APP_SLAVE_COUNT][8];
static AppUartParser parser;

static uint32_t NowMs(void)
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

static void Put16(uint8_t *output, uint16_t value)
{
    output[0] = (uint8_t)value;
    output[1] = (uint8_t)(value >> 8U);
}

static void Put32(uint8_t *output, uint32_t value)
{
    for (unsigned int i = 0U; i < 4U; ++i) {
        output[i] = (uint8_t)(value >> (8U * i));
    }
}

/* USART2 callbacks only: USART1 log callbacks remain untouched. */
static void RxComplete(UART_HandleTypeDef *uart)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (uart != &huart2) {
        return;
    }
    if (xQueueSendFromISR(rxQueue, &rxByte,
                          &higherPriorityTaskWoken) != pdPASS) {
        ++rxDropped;
        rxRestart = true;
    }
    if (HAL_UART_Receive_IT(&huart2, &rxByte, 1U) != HAL_OK) {
        rxRestart = true;
    }
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void TxComplete(UART_HandleTypeDef *uart)
{
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    if (uart == &huart2 && bridgeTask != NULL) {
        vTaskNotifyGiveFromISR(bridgeTask, &higherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

static void UartError(UART_HandleTypeDef *uart)
{
    if (uart == &huart2) {
        ++uartErrors;
        rxRestart = true;
    }
}

static void RestartReception(void)
{
    /* Disable this IRQ while replacing its buffer/state. Other IRQs continue. */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
    rxRestart = false;
    (void)HAL_UART_AbortReceive(&huart2);
    __HAL_UART_CLEAR_FLAG(&huart2,
        UART_CLEAR_PEF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_OREF);
    __HAL_UART_SEND_REQ(&huart2, UART_RXDATA_FLUSH_REQUEST);
    (void)xQueueReset(rxQueue);
    AppUartParser_Reset(&parser);
    if (HAL_UART_Receive_IT(&huart2, &rxByte, 1U) != HAL_OK) {
        rxRestart = true;
    }
    HAL_NVIC_EnableIRQ(USART2_IRQn);
}

static bool SendFrame(uint8_t type, uint8_t sequence,
                      const uint8_t *payload, size_t length)
{
    size_t size = AppUart_Encode(type, sequence, payload, length,
                                 txBuffer, sizeof(txBuffer));
    if (size == 0U) {
        ++txErrors;
        return false;
    }
    /* Only the TX-complete callback uses this task's notification. */
    (void)ulTaskNotifyTake(pdTRUE, 0U);
    if (HAL_UART_Transmit_IT(&huart2, txBuffer, (uint16_t)size) != HAL_OK) {
        ++txErrors;
        return false;
    }
    if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(TX_TIMEOUT_MS)) == 0U) {
        (void)HAL_UART_AbortTransmit(&huart2);
        ++txErrors;
        return false;
    }
    return true;
}

static void Reply(const AppUartFrame *request, uint8_t error)
{
    const uint8_t payload[2] = {request->type, error};
    (void)SendFrame(error == APP_UART_OK_QUEUED
        ? APP_UART_ACK : APP_UART_NACK,
        request->sequence, payload, sizeof(payload));
}

static void HandleFrame(const AppUartFrame *frame)
{
    if (frame->type == APP_UART_ACK || frame->type == APP_UART_NACK) {
        return; /* Never acknowledge an acknowledgement. */
    }
    if (frame->type == APP_UART_PING) {
        if (frame->length != 0U) {
            Reply(frame, APP_UART_ERR_LENGTH);
            return;
        }
        uint8_t payload[6] = {APP_UART_PROTOCOL_VERSION, 0U};
        payload[1] = AppIpc_NetworkStarted() ? 1U : 0U;
        Put32(&payload[2], NowMs());
        (void)SendFrame(APP_UART_PONG, frame->sequence,
                        payload, sizeof(payload));
        return;
    }
    if (frame->type == APP_UART_PERMIT_JOIN) {
        if (frame->length != 1U) {
            Reply(frame, APP_UART_ERR_LENGTH);
        } else if (frame->payload[0] == 255U) {
            Reply(frame, APP_UART_ERR_VALUE);
        } else if (!AppIpc_NetworkStarted() || qZbCommand == NULL) {
            Reply(frame, APP_UART_ERR_NOT_READY);
        } else if (!AppIpc_RequestPermitJoin(frame->payload[0], frame->sequence)) {
            Reply(frame, APP_UART_ERR_QUEUE_FULL);
        } else {
            Reply(frame, APP_UART_OK_QUEUED);
        }
        return;
    }
    /* SET_SETPOINT is not forwarded until the Zigbee control path exists. */
    Reply(frame, APP_UART_ERR_UNSUPPORTED);
}

static void PublishHeartbeat(void)
{
    uint8_t payload[22] = {APP_UART_PROTOCOL_VERSION, 0U};
    payload[1] = AppIpc_NetworkStarted() ? 1U : 0U;
    Put32(&payload[2], NowMs());
    Put32(&payload[6], rxDropped);
    Put32(&payload[10], uartErrors);
    Put32(&payload[14], parser.crc_errors);
    Put32(&payload[18], txErrors);
    (void)SendFrame(APP_UART_HEARTBEAT, txSequence++, payload, sizeof(payload));
}

static void PublishSlave(uint8_t nodeId)
{
    AppStatus status;
    const uint8_t zeroIeee[8] = {0};
    if (!AppIpc_ReadSlaveStatus(nodeId, &status) ||
        memcmp(status.device.ieee_address, zeroIeee, sizeof(zeroIeee)) == 0) {
        return;
    }
    unsigned int index = (unsigned int)nodeId - 1U;
    if (memcmp(announcedIeee[index], status.device.ieee_address, 8U) != 0) {
        uint8_t joined[10] = {APP_UART_PROTOCOL_VERSION, nodeId};
        memcpy(&joined[2], status.device.ieee_address, 8U);
        if (SendFrame(APP_UART_DEVICE_JOINED, txSequence++, joined, sizeof(joined))) {
            memcpy(announcedIeee[index], status.device.ieee_address, 8U);
        }
    }

    uint8_t payload[21] = {APP_UART_PROTOCOL_VERSION, nodeId};
    memcpy(&payload[2], status.device.ieee_address, 8U);
    bool seen = status.updated_at_ms != 0U;
    uint32_t age = seen ? NowMs() - status.updated_at_ms : UINT32_MAX;
    bool stale = seen && age > TEMP_STALE_MS;
    bool valid = seen && !stale &&
        (status.latest_sample.valid & APP_SAMPLE_VALID_TEMP) != 0U;
    payload[10] = (uint8_t)((valid ? APP_UART_TEMP_VALID : 0U) |
        (stale ? APP_UART_TEMP_STALE : 0U) |
        (seen ? APP_UART_TEMP_REPORT_SEEN : 0U));
    Put16(&payload[11], valid
        ? (uint16_t)status.latest_sample.temperature_centi_c : 0U);
    Put32(&payload[13], status.updated_at_ms);
    Put32(&payload[17], age);
    (void)SendFrame(APP_UART_TELEMETRY_TEMP, txSequence++, payload, sizeof(payload));
}

static void BridgeTask(void *argument)
{
    (void)argument;
    AppUartParser_Init(&parser);
    MX_USART2_UART_Init();

    /* IRQ 8 can call FreeRTOS FromISR APIs (project max-syscall priority 5). */
    HAL_NVIC_SetPriority(USART2_IRQn, 8U, 0U);
    if (HAL_UART_RegisterCallback(&huart2, HAL_UART_RX_COMPLETE_CB_ID, RxComplete) != HAL_OK ||
        HAL_UART_RegisterCallback(&huart2, HAL_UART_TX_COMPLETE_CB_ID, TxComplete) != HAL_OK ||
        HAL_UART_RegisterCallback(&huart2, HAL_UART_ERROR_CB_ID, UartError) != HAL_OK) {
        Error_Handler();
        vTaskDelete(NULL);
    }
    RestartReception();
    /* PB7 drives the ON input of the ESP32 TPS22918 load switch. */
    HAL_GPIO_WritePin(ESP32_EN_GPIO_Port, ESP32_EN_Pin, GPIO_PIN_SET);

    TickType_t lastPublication = xTaskGetTickCount();
    TickType_t lastByte = lastPublication;
    for (;;) {
        if (rxRestart) {
            RestartReception();
        }
        uint8_t byte;
        if (xQueueReceive(rxQueue, &byte, pdMS_TO_TICKS(10U)) == pdPASS) {
            TickType_t now = xTaskGetTickCount();
            if ((TickType_t)(now - lastByte) >= pdMS_TO_TICKS(RX_FRAME_GAP_MS)) {
                AppUartParser_Reset(&parser);
            }
            lastByte = now;
            AppUartParser_Push(&parser, byte);
            AppUartFrame frame;
            while (AppUartParser_Next(&parser, &frame)) {
                HandleFrame(&frame);
            }
        }
        TickType_t now = xTaskGetTickCount();
        if ((TickType_t)(now - lastPublication) >= pdMS_TO_TICKS(PUBLICATION_MS)) {
            lastPublication = now;
            PublishHeartbeat();
            for (uint8_t id = 1U; id <= APP_SLAVE_COUNT; ++id) {
                PublishSlave(id);
            }
        }
    }
}

void AppUartBridge_Start(void)
{
    if (bridgeTask != NULL) {
        return;
    }
    rxQueue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(uint8_t));
    if (rxQueue == NULL) {
        Error_Handler();
        return;
    }
    if (xTaskCreate(BridgeTask, "BridgeTask",
                   APP_STACK_DEPTH(APP_TASK_STACK_BRIDGE), NULL,
                   APP_TASK_PRIO_BRIDGE, &bridgeTask) != pdPASS) {
        vQueueDelete(rxQueue);
        rxQueue = NULL;
        Error_Handler();
    }
}

#else

void AppUartBridge_Start(void)
{
    /* Slave does not initialize USART2, power the ESP32, or create BridgeTask. */
}

#endif
