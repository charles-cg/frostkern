#ifndef TEST_MAIN_H
#define TEST_MAIN_H
#include <stdint.h>
#define USE_HAL_UART_REGISTER_CALLBACKS 1
typedef struct { void *Instance; } UART_HandleTypeDef;
typedef enum {HAL_OK, HAL_ERROR, HAL_BUSY, HAL_TIMEOUT} HAL_StatusTypeDef;
typedef enum {HAL_UART_RX_COMPLETE_CB_ID, HAL_UART_TX_COMPLETE_CB_ID,
              HAL_UART_ERROR_CB_ID} HAL_UART_CallbackIDTypeDef;
#define USART2_IRQn 2
#define GPIO_PIN_SET 1
#define ESP32_EN_GPIO_Port ((void *)1)
#define ESP32_EN_Pin 128U
#define BOOST_EN_GPIO_Port ((void *)1)
#define BOOST_EN_Pin 32U
#define UART_CLEAR_PEF 1U
#define UART_CLEAR_FEF 2U
#define UART_CLEAR_NEF 4U
#define UART_CLEAR_OREF 8U
#define UART_RXDATA_FLUSH_REQUEST 1U
#define __HAL_UART_CLEAR_FLAG(handle, flags) ((void)(handle), (void)(flags))
#define __HAL_UART_SEND_REQ(handle, req) ((void)(handle), (void)(req))
void MX_USART2_UART_Init(void);
void Error_Handler(void);
void HAL_NVIC_DisableIRQ(int);
void HAL_NVIC_EnableIRQ(int);
void HAL_NVIC_SetPriority(int, uint32_t, uint32_t);
void HAL_GPIO_WritePin(void *, uint16_t, int);
HAL_StatusTypeDef HAL_UART_RegisterCallback(UART_HandleTypeDef *,
    HAL_UART_CallbackIDTypeDef, void (*)(UART_HandleTypeDef *));
HAL_StatusTypeDef HAL_UART_Receive_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_Transmit_IT(UART_HandleTypeDef *, uint8_t *, uint16_t);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *);
HAL_StatusTypeDef HAL_UART_AbortTransmit(UART_HandleTypeDef *);
#endif
