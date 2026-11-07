/*
 * esp8266.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "esp8266.h"
#include "main.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

/* ---------- RX ring buffer (DMA target) ---------- */

#define ESP_RX_RING_SIZE    2048U

static uint8_t s_rx_ring[ESP_RX_RING_SIZE];
static volatile uint16_t s_rx_tail = 0U;
static volatile uint8_t s_initialized = 0U;

/* Response accumulation buffer for ack matching */
#define ESP_RESP_BUF_SIZE   512U
static char s_resp_buf[ESP_RESP_BUF_SIZE];

/* ---------- RX helpers ---------- */

static uint16_t esp8266_rx_head(void)
{
  /* DMA writes into ring in circular mode */
  uint16_t remaining = (uint16_t) __HAL_DMA_GET_COUNTER(huart1.hdmarx);
  uint16_t head = (uint16_t) (ESP_RX_RING_SIZE - remaining);
  return head % ESP_RX_RING_SIZE;
}

static uint16_t esp8266_rx_available(void)
{
  uint16_t head = esp8266_rx_head();
  uint16_t tail = s_rx_tail;
  if(head >= tail)
  {
    return (uint16_t) (head - tail);
  }
  return (uint16_t) (ESP_RX_RING_SIZE - tail + head);
}

static uint8_t esp8266_rx_read(void)
{
  uint8_t b = s_rx_ring[s_rx_tail];
  s_rx_tail = (uint16_t) ((s_rx_tail + 1U) % ESP_RX_RING_SIZE);
  return b;
}

/* Discard any pending RX */
static void esp8266_rx_flush(void)
{
  s_rx_tail = esp8266_rx_head();
}

/* ---------- Init ---------- */

void ESP8266_Init(void)
{
  s_rx_tail = 0U;
  s_initialized = 0U;

  /* Start DMA circular RX. Once armed, this runs forever. */
  if(HAL_UART_Receive_DMA(&huart1, s_rx_ring, ESP_RX_RING_SIZE) != HAL_OK)
  {
    (void) USART2_Printf("ESP: RX DMA start FAILED\r\n");
    return;
  }

  s_initialized = 1U;
}

/* ---------- Core send + wait-for-ack ---------- */

ESP8266_Status_t ESP8266_SendCommand(const char *cmd, const char *ack, uint32_t timeout_ms)
{
  if((s_initialized == 0U) || (cmd == NULL) || (ack == NULL))
  {
    return ESP8266_NOT_INITIALIZED;
  }

  esp8266_rx_flush();

  size_t cmd_len = strlen(cmd);

  /* Send command over UART (blocking, ~87 µs per byte) */
  if(HAL_UART_Transmit(&huart1, (uint8_t*) cmd, (uint16_t) cmd_len, 500U) != HAL_OK)
  {
    return ESP8266_ERROR;
  }

  /* Clear response buffer */
  s_resp_buf[0] = '\0';
  uint16_t idx = 0U;

  TickType_t start = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

  while((xTaskGetTickCount() - start) < timeout)
  {
    /* Drain everything currently in the ring */
    while(esp8266_rx_available() > 0U)
    {
      uint8_t b = esp8266_rx_read();
      if(idx < (ESP_RESP_BUF_SIZE - 1U))
      {
        s_resp_buf[idx++] = (char) b;
        s_resp_buf[idx] = '\0';
      }
    }

    if(strstr(s_resp_buf, ack) != NULL)
    {
      return ESP8266_OK;
    }
    if(strstr(s_resp_buf, "ERROR") != NULL)
    {
      return ESP8266_ERROR;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return (idx == 0U) ? ESP8266_NO_RESPONSE : ESP8266_TIMEOUT;
}

ESP8266_Status_t ESP8266_SendBinary(const uint8_t *data, uint16_t len, const char *ack, uint32_t timeout_ms)
{
  if((s_initialized == 0U) || (data == NULL) || (ack == NULL))
  {
    return ESP8266_NOT_INITIALIZED;
  }

  esp8266_rx_flush();

  if(HAL_UART_Transmit(&huart1, (uint8_t*) data, len, 1000U) != HAL_OK)
  {
    return ESP8266_ERROR;
  }

  s_resp_buf[0] = '\0';
  uint16_t idx = 0U;

  TickType_t start = xTaskGetTickCount();
  TickType_t timeout = pdMS_TO_TICKS(timeout_ms);

  while((xTaskGetTickCount() - start) < timeout)
  {
    while(esp8266_rx_available() > 0U)
    {
      uint8_t b = esp8266_rx_read();
      if(idx < (ESP_RESP_BUF_SIZE - 1U))
      {
        s_resp_buf[idx++] = (char) b;
        s_resp_buf[idx] = '\0';
      }
    }

    if(strstr(s_resp_buf, ack) != NULL)
    {
      return ESP8266_OK;
    }
    if(strstr(s_resp_buf, "ERROR") != NULL)
    {
      return ESP8266_ERROR;
    }

    vTaskDelay(pdMS_TO_TICKS(1));
  }

  return (idx == 0U) ? ESP8266_NO_RESPONSE : ESP8266_TIMEOUT;
}

/* ---------- Connectivity ---------- */

ESP8266_Status_t ESP8266_Test(void)
{
  esp8266_rx_flush();

  /* Give module time after power-up / reset */
  vTaskDelay(pdMS_TO_TICKS(500));

  if(ESP8266_SendCommand("AT\r\n", "OK\r\n", 2000U) != ESP8266_OK)
  {
    return ESP8266_ERROR;
  }

  if(ESP8266_SendCommand("ATE0\r\n", "OK\r\n", 2000U) != ESP8266_OK)
  {
    return ESP8266_ERROR;
  }

  return ESP8266_OK;
}

ESP8266_Status_t ESP8266_ConnectWiFi(const char *ssid, const char *password, uint32_t timeout_ms)
{
  char cmd[128];

  esp8266_rx_flush();

  /* Station mode */
  if(ESP8266_SendCommand("AT+CWMODE=1\r\n", "OK\r\n", 2000U) != ESP8266_OK)
  {
    return ESP8266_ERROR;
  }

  /* Join network — this can take several seconds */
  int n = snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, password);
  if((n <= 0) || (n >= (int) sizeof(cmd)))
  {
    return ESP8266_ERROR;
  }

  ESP8266_Status_t s = ESP8266_SendCommand(cmd, "WIFI GOT IP", timeout_ms);
  if(s != ESP8266_OK)
  {
    /* Some firmware prints "WIFI CONNECTED" then "OK" without "WIFI GOT IP" */
    s = ESP8266_SendCommand("AT+CIFSR\r\n", "OK\r\n", 3000U);
  }
  return s;
}

ESP8266_Status_t ESP8266_GetIP(char *ip_buf, uint16_t buf_len)
{
  if((ip_buf == NULL) || (buf_len < 8U))
  {
    return ESP8266_ERROR;
  }

  for(uint8_t attempt = 1U; attempt <= 3U; attempt++)
  {
    esp8266_rx_flush();

    if(ESP8266_SendCommand("AT+CIFSR\r\n", "OK\r\n", 5000U) != ESP8266_OK)
    {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    char *p = strstr(s_resp_buf, "STAIP,\"");
    if(p == NULL)
    {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    p += 7; /* skip "STAIP,\"" */

    char *end = strchr(p, '"');
    if(end == NULL)
    {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    size_t len = (size_t) (end - p);
    if((len == 0U) || (len >= (size_t) buf_len))
    {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    /* IP not ready yet — DHCP still finalizing */
    if((len == 7U) && (strncmp(p, "0.0.0.0", 7) == 0))
    {
      vTaskDelay(pdMS_TO_TICKS(1000));
      continue;
    }

    (void) memcpy(ip_buf, p, len);
    ip_buf[len] = '\0';
    return ESP8266_OK;
  }

  return ESP8266_ERROR;
}

ESP8266_Status_t ESP8266_TCPConnect(const char *host, uint16_t port, uint32_t timeout_ms)
{
  if(host == NULL)
  {
    return ESP8266_ERROR;
  }

  /* Close any previous connection first (ignore result — may be "no link") */
  (void) ESP8266_SendCommand("AT+CIPCLOSE\r\n", "OK\r\n", 2000U);
  esp8266_rx_flush();

  char cmd[128];
  int n = snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n", host, (unsigned) port);
  if((n <= 0) || (n >= (int) sizeof(cmd)))
  {
    return ESP8266_ERROR;
  }

  return ESP8266_SendCommand(cmd, "CONNECT\r\n", timeout_ms);
}
