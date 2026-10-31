/*
 * sensor_data.h
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#ifndef UTILS_SENSOR_DATA_H_
#define UTILS_SENSOR_DATA_H_

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

typedef struct {
    uint8_t  sec;      /* 0-59 */
    uint8_t  min;      /* 0-59 */
    uint8_t  hour;     /* 0-23 (24-hour mode) */
    uint8_t  dow;      /* 1-7 */
    uint8_t  date;     /* 1-31 */
    uint8_t  month;    /* 1-12 */
    uint8_t  year;     /* 0-99 */
} RTC_Time_t;

typedef struct {
    /* ---- DHT11 ---- */
    float      temp_c;         /* °C */
    float      hum_pct;        /* %RH */
    TickType_t dht_tick;       /* xTaskGetTickCount() when last written */
    uint8_t    dht_valid;      /* 1 = has been read at least once */

    /* ---- MPU6050 ---- */
    float      ax_g, ay_g, az_g;      /* g */
    float      gx_dps, gy_dps, gz_dps;/* °/s */
    TickType_t imu_tick;
    uint8_t    imu_valid;

    /* ---- DS3231 ---- */
    RTC_Time_t rtc;
    TickType_t rtc_tick;
    uint8_t    rtc_valid;
} SensorData_t;

extern SensorData_t      g_sensor;
extern SemaphoreHandle_t g_sensor_mtx;

void SensorData_Init(void);
void SensorData_GetSnapshot(SensorData_t *out);

#endif /* UTILS_SENSOR_DATA_H_ */
