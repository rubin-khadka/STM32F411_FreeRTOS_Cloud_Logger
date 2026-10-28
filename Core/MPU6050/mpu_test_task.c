/*
 * mpu_test_task.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "mpu_test_task.h"
#include "mpu6050.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

#define MPU_PERIOD_MS  50U    /* 20 Hz */

void vMpuTestTask(void *pv)
{
  (void) pv;

  /* Init once */
  MPU6050_Status_t init_status = MPU6050_Init();
  if(init_status != MPU6050_OK)
  {
    (void) USART2_Printf("MPU: init failed (status=%d)\r\n", (int) init_status);
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }
  (void) USART2_Printf("MPU: init OK\r\n");

  TickType_t last_wake = xTaskGetTickCount();
  MPU6050_Data_t d;

  for(;;)
  {
    if(MPU6050_Read(&d) == MPU6050_OK)
    {
      /* newlib-nano has no %f — scale to integers for printing. */
      int32_t ax_mg = (int32_t) (d.ax_g * 1000.0f);
      int32_t ay_mg = (int32_t) (d.ay_g * 1000.0f);
      int32_t az_mg = (int32_t) (d.az_g * 1000.0f);
      int32_t gx_dp = (int32_t) (d.gx_dps * 10.0f);
      int32_t gy_dp = (int32_t) (d.gy_dps * 10.0f);
      int32_t gz_dp = (int32_t) (d.gz_dps * 10.0f);
      int32_t t_cc = (int32_t) (d.temp_c * 100.0f);

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
    else
    {
      (void) USART2_Printf("MPU: read failed\r\n");
    }

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MPU_PERIOD_MS));
  }
}
