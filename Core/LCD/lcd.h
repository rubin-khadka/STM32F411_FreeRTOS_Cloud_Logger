/*
 * lcd.h
 *
 *  Created on: Oct 29, 2026
 *      Author: Rubin Khadka
 */

#ifndef LCD_LCD_H_
#define LCD_LCD_H_

#include <stdint.h>

/* PCF8574 backpack I2C address (7-bit). */
#define LCD_ADDR_7BIT    (0x27U)

typedef enum
{
  LCD_OK = 0,
  LCD_ERR_INVALID_ARG,
  LCD_ERR_IO
} LCD_Status_t;

LCD_Status_t LCD_Init(void); /* must run inside a task */
void LCD_Clear(void);
void LCD_SetCursor(uint8_t row, uint8_t col); /* row: 0 or 1, col: 0–15 */
void LCD_SendChar(char c);
void LCD_SendString(const char *str);

#endif /* LCD_LCD_H_ */
