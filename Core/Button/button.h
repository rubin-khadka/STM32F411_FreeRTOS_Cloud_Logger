/*
 * button.h
 *
 *  Created on: Oct 30, 2026
 *      Author: Rubin Khadka
 */

#ifndef BUTTON_BUTTON_H_
#define BUTTON_BUTTON_H_

#include <stdint.h>

/* Display mode */
typedef enum
{
  DISPLAY_MODE_TEMP_HUM = 0,
  DISPLAY_MODE_ACCEL = 1,
  DISPLAY_MODE_GYRO = 2,
  DISPLAY_MODE_DATETIME = 3,
  DISPLAY_MODE_COUNT = 4
} DisplayMode_t;

/* Button Initialization */
void Button_Init(void);
DisplayMode_t Button_GetMode(void);   /* Current display mode */
void Button_NextMode(void);

#endif /* BUTTON_BUTTON_H_ */
