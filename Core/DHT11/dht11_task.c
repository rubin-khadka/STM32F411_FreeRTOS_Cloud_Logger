/*
 * dht11_task.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "dht11.h"
#include "dht11_task.h"
#include "sensor_data.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

#define DHT_PERIOD_MS  1000U

void vDht11Task(void *pv)
{
    (void) pv;

    DHT11_Init();

    TickType_t     last_wake = xTaskGetTickCount();
    DHT11_Data_t   sensor_reading;
    DHT11_Status_t read_status;

    for (;;)
    {
        read_status = DHT11_ReadFrame(&sensor_reading);

        if (read_status == DHT11_OK)
        {
            /* Publish to shared snapshot — silent on success */
            xSemaphoreTake(g_sensor_mtx, portMAX_DELAY);
            g_sensor.temp_c    = sensor_reading.temp_c;
            g_sensor.hum_pct   = sensor_reading.hum_pct;
            g_sensor.dht_tick  = xTaskGetTickCount();
            g_sensor.dht_valid = 1U;
            xSemaphoreGive(g_sensor_mtx);
        }
        else
        {
            /* Failure */
            (void) USART2_Printf("DHT: read failed (status=%d)\r\n", (int) read_status);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(DHT_PERIOD_MS));
    }
}
