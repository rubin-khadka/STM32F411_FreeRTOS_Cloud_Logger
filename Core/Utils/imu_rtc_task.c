/*
 * imu_rtc_task.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */

#include "imu_rtc_task.h"

#include "mpu6050.h"
#include "ds3231.h"
#include "sensor_data.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

#define IMU_PERIOD_MS        50U     /* 20 Hz  */
#define RTC_DIVIDER_COUNT    20U     /* 20 × 50 ms = 1000 ms */

/* Used only when OSF is set (first boot / battery loss). */
#define INITIAL_HOUR   14U
#define INITIAL_MIN    30U
#define INITIAL_SEC    0U
#define INITIAL_DATE   28U
#define INITIAL_MONTH  10U
#define INITIAL_YEAR   26U     /* 2026 */
#define INITIAL_DOW    3U

static void set_initial_time(void)
{
    DS3231_Time_t t = {
        .sec   = INITIAL_SEC,
        .min   = INITIAL_MIN,
        .hour  = INITIAL_HOUR,
        .dow   = INITIAL_DOW,
        .date  = INITIAL_DATE,
        .month = INITIAL_MONTH,
        .year  = INITIAL_YEAR
    };

    if (DS3231_SetTime(&t) != DS3231_OK)
    {
        /* Failure */
        (void) USART2_Printf("RTC: set time FAILED\r\n");
    }
}

void vImuRtcTask(void *pv)
{
    (void) pv;

    /* --- Init both sensors independently --- */
    MPU6050_Status_t imu_status = MPU6050_Init();
    if (imu_status != MPU6050_OK)
    {
        (void) USART2_Printf("IMU: init FAILED (status=%d)\r\n", (int) imu_status);
    }

    DS3231_Status_t rtc_status = DS3231_Init();
    if (rtc_status == DS3231_ERR_TIME_INVALID)
    {
        set_initial_time();
    }
    else if (rtc_status != DS3231_OK)
    {
        (void) USART2_Printf("RTC: init FAILED (status=%d)\r\n", (int) rtc_status);
    }

    /* --- Main loop: 50 ms period, RTC every 20th iteration --- */
    TickType_t     last_wake   = xTaskGetTickCount();
    uint32_t       rtc_divider = 0U;
    MPU6050_Data_t imu;
    DS3231_Time_t  now;

    for (;;)
    {
        /* MPU6050 — every 50 ms (20 Hz) */
        if (MPU6050_Read(&imu) == MPU6050_OK)
        {
            xSemaphoreTake(g_sensor_mtx, portMAX_DELAY);
            g_sensor.ax_g      = imu.ax_g;
            g_sensor.ay_g      = imu.ay_g;
            g_sensor.az_g      = imu.az_g;
            g_sensor.gx_dps    = imu.gx_dps;
            g_sensor.gy_dps    = imu.gy_dps;
            g_sensor.gz_dps    = imu.gz_dps;
            g_sensor.imu_tick  = xTaskGetTickCount();
            g_sensor.imu_valid = 1U;
            xSemaphoreGive(g_sensor_mtx);
        }

        /* DS3231 — every 20th iteration (1 Hz) */
        if (++rtc_divider >= RTC_DIVIDER_COUNT)
        {
            rtc_divider = 0U;

            if (DS3231_ReadTime(&now) == DS3231_OK)
            {
                xSemaphoreTake(g_sensor_mtx, portMAX_DELAY);
                g_sensor.rtc.sec   = now.sec;
                g_sensor.rtc.min   = now.min;
                g_sensor.rtc.hour  = now.hour;
                g_sensor.rtc.dow   = now.dow;
                g_sensor.rtc.date  = now.date;
                g_sensor.rtc.month = now.month;
                g_sensor.rtc.year  = now.year;
                g_sensor.rtc_tick  = xTaskGetTickCount();
                g_sensor.rtc_valid = 1U;
                xSemaphoreGive(g_sensor_mtx);
            }
            else
            {
                (void) USART2_Printf("RTC: read failed\r\n");
            }
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(IMU_PERIOD_MS));
    }
}
