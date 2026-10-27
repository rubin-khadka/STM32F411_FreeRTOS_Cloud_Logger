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

void vDht11Task(void *pv)
{
  (void) pv;

  DHT11_Init();

  TickType_t last_wake = xTaskGetTickCount();
  DHT11_Data_t sensor_reading;
  DHT11_Status_t read_status;

  for(;;)
  {
    read_status = DHT11_ReadFrame(&sensor_reading);

    if(read_status == DHT11_OK)
    {
      /* Scale to tenths, format as "23.5" without %f */
      int temp_tenths = (int) (sensor_reading.temp_c * 10.0f + 0.5f);
      int hum_tenths = (int) (sensor_reading.hum_pct * 10.0f + 0.5f);

      (void) USART2_Printf(
          "DHT: T=%d.%d C  H=%d.%d %%\r\n",
          temp_tenths / 10,
          temp_tenths % 10,
          hum_tenths / 10,
          hum_tenths % 10);
    }
    else
    {
      (void) USART2_Printf("DHT: read failed (status=%d)\r\n", (int) read_status);
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DHT_PERIOD_MS));
  }
}
