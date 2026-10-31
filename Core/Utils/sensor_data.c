/*
 * sensor_data.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "sensor_data.h"
#include <string.h>

SensorData_t      g_sensor;

SemaphoreHandle_t g_sensor_mtx = NULL;

void SensorData_Init(void)
{
    g_sensor_mtx = xSemaphoreCreateMutex();
    configASSERT(g_sensor_mtx != NULL);
}

void SensorData_GetSnapshot(SensorData_t *out)
{
    if (out == NULL) {
        return;
    }

    xSemaphoreTake(g_sensor_mtx, portMAX_DELAY);
    *out = g_sensor;
    xSemaphoreGive(g_sensor_mtx);
}
