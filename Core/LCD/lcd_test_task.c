/*
 * lcd_test_task.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */


#include "lcd_test_task.h"
#include "lcd.h"
#include "usart2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

#define LCD_TEST_PERIOD_MS  2000U

extern I2C_HandleTypeDef hi2c1;

void vLcdTestTask(void *pv)
{
  (void) pv;

  /* Scan I2C1 — logs every responding address. */
  uint8_t first_found = 0U;
  for(uint8_t addr = 1U; addr < 0x7FU; addr++)
  {
    if(HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t) (addr << 1U), 3U, 50U) == HAL_OK)
    {
      (void) USART2_Printf("I2C1 scan: device at 0x%02X\r\n", (unsigned int) addr);
      if(first_found == 0U)
      {
        first_found = addr;
      }
    }
  }

  if(first_found == 0U)
  {
    (void) USART2_Printf("LCD: no device found on I2C1\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  (void) USART2_Printf("LCD: using 0x%02X (update LCD_ADDR_7BIT if different)\r\n", (unsigned int) LCD_ADDR_7BIT);

  LCD_Init();

  TickType_t last_wake = xTaskGetTickCount();
  uint8_t screen = 0U;

  for(;;)
  {
    LCD_Clear();

    switch(screen)
    {
      case 0U:
        LCD_SetCursor(0U, 0U);
        LCD_SendString("LCD Test OK");
        LCD_SetCursor(1U, 0U);
        LCD_SendString("I2C1 100kHz");
        break;

      case 1U:
        LCD_SetCursor(0U, 0U);
        LCD_SendString("0123456789ABCDEF");
        LCD_SetCursor(1U, 0U);
        LCD_SendString("Line 2 16 chars");
        break;

      case 2U:
        LCD_SetCursor(0U, 0U);
        LCD_SendString("Cursor test:");
        LCD_SetCursor(1U, 4U);
        LCD_SendString("Centered");
        break;

      default:
        break;
    }

    screen = (uint8_t) ((screen + 1U) % 3U);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LCD_TEST_PERIOD_MS));
  }
}
