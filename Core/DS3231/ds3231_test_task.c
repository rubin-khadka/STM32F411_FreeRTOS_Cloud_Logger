/*
 * ds3231_test_task.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */

#include "ds3231_test_task.h"
#include "ds3231.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

/* Set these to your PC clock at the moment you flash.
 * Only used when OSF is set (first boot or after battery loss). */
#define INITIAL_HOUR   14U
#define INITIAL_MIN    30U
#define INITIAL_SEC    0U
#define INITIAL_DATE   28U
#define INITIAL_MONTH  10U
#define INITIAL_YEAR   26U     /* 2026 */
#define INITIAL_DOW    3U      /* arbitrary: 1=Mon ... 7=Sun */

#define RTC_TEST_PERIOD_MS  1000U

void vDs3231TestTask(void *pv)
{
  (void) pv;

  DS3231_Status_t s = DS3231_Init();

  if(s == DS3231_ERR_NOT_FOUND)
  {
    (void) USART2_Printf("RTC: init failed (no device at 0x68)\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  if(s == DS3231_ERR_TIME_INVALID)
  {
    /* First boot or battery lost — set the time once. */
    (void) USART2_Printf(
        "RTC: OSF set, setting time to %02d-%02d-%02d %02d:%02d:%02d\r\n",
        (int) INITIAL_YEAR,
        (int) INITIAL_MONTH,
        (int) INITIAL_DATE,
        (int) INITIAL_HOUR,
        (int) INITIAL_MIN,
        (int) INITIAL_SEC);

    DS3231_Time_t t = { .sec = INITIAL_SEC, .min = INITIAL_MIN, .hour = INITIAL_HOUR, .dow = INITIAL_DOW, .date =
        INITIAL_DATE, .month = INITIAL_MONTH, .year = INITIAL_YEAR };

    if(DS3231_SetTime(&t) == DS3231_OK)
    {
      (void) USART2_Printf("RTC: time set OK\r\n");
    }
    else
    {
      (void) USART2_Printf("RTC: time set FAILED\r\n");
    }
  }
  else if(s == DS3231_OK)
  {
    (void) USART2_Printf("RTC: init OK (time was valid)\r\n");
  }
  else
  {
    (void) USART2_Printf("RTC: init error (status=%d)\r\n", (int) s);
  }

  TickType_t last_wake = xTaskGetTickCount();
  DS3231_Time_t now;

  for(;;)
  {
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

    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(RTC_TEST_PERIOD_MS));
  }
}
