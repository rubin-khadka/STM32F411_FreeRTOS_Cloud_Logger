/*
 * dht11_task.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "dht11_task.h"
#include "dht11.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

#define DHT_PERIOD_MS  1000U

void vDhtTask(void *pv)
{
  (void) pv;

  DHT11_Init();

  TickType_t last = xTaskGetTickCount();
  DHT11_Data_t dht11_data;

  for(;;)
  {
    DHT11_Status_t dht11_status = DHT11_ReadFrame(&dht11_data);

    if(dht11_status == DHT11_OK)
    {
      (void) USART2_Printf("DHT: T=%.1f C  H=%.1f %%\r\n", (double) dht11_data.temp_c, (double) dht11_data.hum_pct);
    }
    else
    {
      (void) USART2_Printf("DHT: read failed (status=%d)\r\n", (int) dht11_status);
    }

    vTaskDelayUntil(&last, pdMS_TO_TICKS(DHT_PERIOD_MS));
  }
}
