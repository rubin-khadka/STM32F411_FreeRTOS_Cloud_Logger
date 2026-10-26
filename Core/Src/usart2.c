/*
 * usart2.c
 *
 *  Created on: Oct 26, 2026
 *      Author: Rubin Khadka
 */

#include "usart2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

extern UART_HandleTypeDef huart2;

#define USART2_TX_BUF_SIZE      256U
#define USART2_TX_TIMEOUT_MS    1000U

/* One internal TX buffer, One Semaphore */
static uint8_t s_tx_buf[USART2_TX_BUF_SIZE];
static SemaphoreHandle_t s_tx_free = NULL;

void USART2_Init(void)
{
  s_tx_free = xSemaphoreCreateBinary();
  configASSERT(s_tx_free != NULL);
  xSemaphoreGive(s_tx_free); /* buffer starts free */
}

USART2_Status_t USART2_Write(const uint8_t *buf, size_t len)
{
  configASSERT(s_tx_free != NULL); /* catch "called before Init" */

  if(buf == NULL)
    return USART2_ERR_INVALID_ARG;
  if(len == 0U)
    return USART2_OK; /* no-op */
  if(len > USART2_TX_BUF_SIZE)
    len = USART2_TX_BUF_SIZE; /* clamp oversized */

  /* Blocks only if the previous message is still in flight. */
  if(xSemaphoreTake(s_tx_free, pdMS_TO_TICKS(USART2_TX_TIMEOUT_MS)) != pdTRUE)
    return USART2_ERR_TIMEOUT;

  memcpy(s_tx_buf, buf, len); /* copy so caller's buf can be reused */

  if(HAL_UART_Transmit_DMA(&huart2, s_tx_buf, (uint16_t) len) != HAL_OK)
  {
    xSemaphoreGive(s_tx_free); /* release on failure */
    return USART2_ERR_HAL;
  }
  return USART2_OK; /* DMA in flight; TC callback releases */
}

USART2_Status_t USART2_WriteString(const char *str)
{
  if(str == NULL)
    return USART2_ERR_INVALID_ARG;
  return USART2_Write((const uint8_t*) str, strlen(str));
}

USART2_Status_t USART2_Printf(const char *fmt, ...)
{
  if(fmt == NULL)
    return USART2_ERR_INVALID_ARG;

  char buf[USART2_TX_BUF_SIZE];
  va_list args;
  va_start(args, fmt);
  int n = vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  if(n < 0)
    return USART2_ERR_INVALID_ARG; /* format error */
  if(n == 0)
    return USART2_OK; /* nothing to print */

  /* vsnprintf returns wanted length, which can exceed buf on truncation. */
  size_t len = (n < (int) sizeof(buf)) ? (size_t) n : sizeof(buf) - 1U;
  return USART2_Write((const uint8_t*) buf, len);
}

USART2_Status_t USART2_Flush(uint32_t timeout_ms)
{
  configASSERT(s_tx_free != NULL);

  /* Take + give proves the buffer is free without consuming the token. */
  if(xSemaphoreTake(s_tx_free, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
    return USART2_ERR_TIMEOUT;
  xSemaphoreGive(s_tx_free);
  return USART2_OK;
}

/* HAL weak callbacks. Only ONE definition allowed project-wide.
 * When USART1 (ESP8266) is added, extend with `else if`, do not redefine. */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART2)
  {
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(s_tx_free, &hpw); /* release buffer */
    portYIELD_FROM_ISR(hpw); /* wake blocked writer now */
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if(huart->Instance == USART2)
  {
    /* DMA aborted — release to avoid permanent stall on next Write. */
    BaseType_t hpw = pdFALSE;
    xSemaphoreGiveFromISR(s_tx_free, &hpw);
    portYIELD_FROM_ISR(hpw);
  }
}
