/*
 * lcd.c
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */

#include "lcd.h"
#include "main.h"
#include "dwt.h"
#include "FreeRTOS.h"
#include "task.h"

extern I2C_HandleTypeDef hi2c1;

#define LCD_ADDR_8BIT     ((uint16_t)(LCD_ADDR_7BIT << 1U))
#define LCD_TIMEOUT_MS    (50U)

/* PCF8574 bit assignments on the backpack */
#define LCD_BACKLIGHT     (0x08U)
#define LCD_ENABLE        (0x04U)
#define LCD_RW            (0x02U)   /* unused — always write mode */
#define LCD_RS            (0x01U)

/* HD44780 command opcodes (subset used by init) */
#define LCD_CMD_CLEAR         (0x01U)
#define LCD_CMD_HOME          (0x02U)
#define LCD_CMD_ENTRY_MODE    (0x06U)
#define LCD_CMD_DISPLAY_OFF   (0x08U)
#define LCD_CMD_DISPLAY_ON    (0x0CU)
#define LCD_CMD_4BIT_2LINE    (0x28U)

static LCD_Status_t lcd_write_byte(uint8_t byte)
{
  HAL_StatusTypeDef st = HAL_I2C_Master_Transmit(&hi2c1,
  LCD_ADDR_8BIT, &byte, 1U,
  LCD_TIMEOUT_MS);
  return (st == HAL_OK) ? LCD_OK : LCD_ERR_IO;
}

/* One nibble + EN strobe = 2 I2C writes */
static void lcd_send_nibble(uint8_t nibble, uint8_t rs)
{
  uint8_t data = (uint8_t) (nibble | LCD_BACKLIGHT);
  if(rs != 0U)
  {
    data |= LCD_RS;
  }

  /* EN high */
  (void) lcd_write_byte((uint8_t) (data | LCD_ENABLE));
  DWT_Delay_us(2U); /* EN pulse width (spec ≥ 450 ns) */

  /* EN low */
  (void) lcd_write_byte((uint8_t) (data & (uint8_t) (~LCD_ENABLE)));
}

static void lcd_send_cmd(uint8_t cmd)
{
  lcd_send_nibble((uint8_t) (cmd & 0xF0U), 0U);
  lcd_send_nibble((uint8_t) ((cmd << 4U) & 0xF0U), 0U);

  /* CLEAR and HOME need long execution time */
  if((cmd == LCD_CMD_CLEAR) || (cmd == LCD_CMD_HOME))
  {
    vTaskDelay(pdMS_TO_TICKS(2)); /* spec: 1.52 ms */
  }
  else
  {
    DWT_Delay_us(50U); /* spec: 37 µs */
  }
}

static void lcd_send_data(uint8_t data)
{
  lcd_send_nibble((uint8_t) (data & 0xF0U), 1U);
  lcd_send_nibble((uint8_t) ((data << 4U) & 0xF0U), 1U);
  DWT_Delay_us(50U);
}

LCD_Status_t LCD_Init(void)
{
  /* Power-up delay */
  vTaskDelay(pdMS_TO_TICKS(100));

  /* HD44780 reset sequence — three 8-bit mode, then switch to 4-bit */
  lcd_send_nibble(0x30U, 0U);
  vTaskDelay(pdMS_TO_TICKS(5));
  lcd_send_nibble(0x30U, 0U);
  DWT_Delay_us(200U);
  lcd_send_nibble(0x30U, 0U);
  DWT_Delay_us(200U);
  lcd_send_nibble(0x20U, 0U); /* switch to 4-bit mode */
  DWT_Delay_us(200U);

  lcd_send_cmd(LCD_CMD_4BIT_2LINE);
  lcd_send_cmd(LCD_CMD_DISPLAY_OFF);
  lcd_send_cmd(LCD_CMD_CLEAR);
  lcd_send_cmd(LCD_CMD_ENTRY_MODE);
  lcd_send_cmd(LCD_CMD_DISPLAY_ON);

  return LCD_OK;
}

void LCD_Clear(void)
{
  lcd_send_cmd(LCD_CMD_CLEAR);
}

void LCD_SetCursor(uint8_t row, uint8_t col)
{
  uint8_t base = (row == 0U) ? 0x80U : 0xC0U;
  lcd_send_cmd((uint8_t) (base + col));
}

void LCD_SendChar(char c)
{
  lcd_send_data((uint8_t) c);
}

void LCD_SendString(const char *str)
{
  if(str == NULL)
  {
    return;
  }
  while(*str != '\0')
  {
    lcd_send_data((uint8_t) *str);
    str++;
  }
}
