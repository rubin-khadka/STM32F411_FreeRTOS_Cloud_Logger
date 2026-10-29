/*
 * imu_rtc_task.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */

#include "imu_rtc_task.h"
#include "mpu6050.h"
#include "ds3231.h"
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
  DS3231_Time_t t = { .sec = INITIAL_SEC, .min = INITIAL_MIN, .hour = INITIAL_HOUR, .dow = INITIAL_DOW, .date =
      INITIAL_DATE, .month = INITIAL_MONTH, .year = INITIAL_YEAR };

  (void) USART2_Printf(
      "RTC: OSF set, setting time to 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
      (int) INITIAL_YEAR,
      (int) INITIAL_MONTH,
      (int) INITIAL_DATE,
      (int) INITIAL_HOUR,
      (int) INITIAL_MIN,
      (int) INITIAL_SEC);

  if(DS3231_SetTime(&t) == DS3231_OK)
  {
    (void) USART2_Printf("RTC: time set OK\r\n");
  }
  else
  {
    (void) USART2_Printf("RTC: time set FAILED\r\n");
  }
}

void vImuRtcTask(void *pv)
{
  (void) pv;

  /* --- Init both sensors independently --- */
  MPU6050_Status_t imu_status = MPU6050_Init();
  if(imu_status == MPU6050_OK)
  {
    (void) USART2_Printf("IMU: init OK\r\n");
  }
  else
  {
    (void) USART2_Printf("IMU: init FAILED (status=%d) — continuing\r\n", (int) imu_status);
  }

  DS3231_Status_t rtc_status = DS3231_Init();
  if(rtc_status == DS3231_ERR_TIME_INVALID)
  {
    set_initial_time();
  }
  else if(rtc_status == DS3231_OK)
  {
    (void) USART2_Printf("RTC: init OK (time was valid)\r\n");
  }
  else
  {
    (void) USART2_Printf("RTC: init FAILED (status=%d) — continuing\r\n", (int) rtc_status);
  }

  /* --- Main loop: 50 ms period, RTC every 20th iteration --- */
  TickType_t last_wake = xTaskGetTickCount();
  uint32_t rtc_divider = 0U;
  MPU6050_Data_t imu;
  DS3231_Time_t now;

  for(;;)
  {
    /* MPU6050 — every 50 ms (20 Hz) */
    if(MPU6050_Read(&imu) == MPU6050_OK)
    {
      int32_t ax_mg = (int32_t) (imu.ax_g * 1000.0f);
      int32_t ay_mg = (int32_t) (imu.ay_g * 1000.0f);
      int32_t az_mg = (int32_t) (imu.az_g * 1000.0f);
      int32_t gx_dp = (int32_t) (imu.gx_dps * 10.0f);
      int32_t gy_dp = (int32_t) (imu.gy_dps * 10.0f);
      int32_t gz_dp = (int32_t) (imu.gz_dps * 10.0f);
      int32_t t_cc = (int32_t) (imu.temp_c * 100.0f);

      (void) USART2_Printf(
          "MPU: a=(%ld,%ld,%ld) mg  g=(%ld,%ld,%ld) d/s  T=%ld.%02ld\r\n",
          (long) ax_mg,
          (long) ay_mg,
          (long) az_mg,
          (long) gx_dp,
          (long) gy_dp,
          (long) gz_dp,
          (long) (t_cc / 100),
          (long) (t_cc % 100));
    }

    /* DS3231 — every 20th iteration (1 Hz) */
    if(++rtc_divider >= RTC_DIVIDER_COUNT)
    {
      rtc_divider = 0U;

      if(DS3231_ReadTime(&now) == DS3231_OK)
      {
        (void) USART2_Printf(
            "RTC: 20%02d-%02d-%02d %02d:%02d:%02d\r\n",
            (int) now.year,
            (int) now.month,
            (int) now.date,
            (int) now.hour,
            (int) now.min,
            (int) now.sec);
      }
      else
      {
        (void) USART2_Printf("RTC: read failed\r\n");
      }
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(IMU_PERIOD_MS));
  }
}
