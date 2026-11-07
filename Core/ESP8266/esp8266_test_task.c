/*
 * esp8266_test_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "esp8266_test_task.h"
#include "esp8266.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

#define WIFI_SSID    "FASTWEB-FBD93D"
#define WIFI_PASS    "CF6N68GNN8"

#define WIFI_TIMEOUT_MS   20000U

/* ============================================================ */

void vEspTestTask(void *pv)
{
  (void) pv;

  vTaskDelay(pdMS_TO_TICKS(2000));

  ESP8266_Init();

  /* ---- Test 1: raw AT ---- */
  ESP8266_Status_t s = ESP8266_SendCommand("AT\r\n", "OK\r\n", 2000U);
  (void) USART2_Printf("ESP: raw AT = %d\r\n", (int) s);
  if(s != ESP8266_OK)
  {
    (void) USART2_Printf("ESP: driver FAILED\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  /* ---- Test 2: disable echo ---- */
  s = ESP8266_SendCommand("ATE0\r\n", "OK\r\n", 2000U);
  if(s != ESP8266_OK)
  {
    (void) USART2_Printf("ESP: ATE0 failed (%d)\r\n", (int) s);
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  /* ---- Test 3: station mode ---- */
  s = ESP8266_SendCommand("AT+CWMODE=1\r\n", "OK\r\n", 2000U);
  if(s != ESP8266_OK)
  {
    (void) USART2_Printf("ESP: CWMODE failed (%d)\r\n", (int) s);
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  /* ---- Test 4: join WiFi ---- */
  (void) USART2_Printf("ESP: joining \"%s\"...\r\n", WIFI_SSID);
  s = ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS, WIFI_TIMEOUT_MS);
  (void) USART2_Printf("ESP: wifi join = %d\r\n", (int) s);
  if(s != ESP8266_OK)
  {
    (void) USART2_Printf("ESP: WiFi FAILED\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  /* ---- Test 5: read IP ---- */
  char ip[32] = { 0 };
  s = ESP8266_GetIP(ip, sizeof(ip));
  if(s != ESP8266_OK)
  {
    (void) USART2_Printf("ESP: GetIP FAILED\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
  (void) USART2_Printf("ESP: ip = %s\r\n", ip);

  (void) USART2_Printf("ESP: ALL TESTS PASSED\r\n");

  for(;;)
  {
    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}
