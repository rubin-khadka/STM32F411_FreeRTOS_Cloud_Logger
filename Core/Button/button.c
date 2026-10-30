/*
 * button.c
 *
 *  Created on: Oct 30, 2026
 *      Author: Rubin Khadka
 */

#include "button.h"
#include "main.h"
#include "FreeRTOS.h"
#include "queue.h"

QueueHandle_t g_button_q = NULL;

static volatile DisplayMode_t current_mode = DISPLAY_MODE_TEMP_HUM;

void Button_Init(void)
{
    g_button_q = xQueueCreate(4U, sizeof(uint8_t));
    configASSERT(g_button_q != NULL);
}

DisplayMode_t Button_GetMode(void)
{
    return current_mode;
}

void Button_NextMode(void)
{
    uint8_t next = (uint8_t)current_mode + 1U;
    if (next >= (uint8_t)DISPLAY_MODE_COUNT) {
        next = (uint8_t)DISPLAY_MODE_TEMP_HUM;
    }
    current_mode = (DisplayMode_t)next;
}

/* HAL EXTI callback */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (g_button_q == NULL) {
        return;   /* not yet initialized */
    }

    uint8_t btn = 0U;
    if (GPIO_Pin == BTN1_Pin) {
        btn = 1U;
    } else if (GPIO_Pin == BTN2_Pin) {
        btn = 2U;
    } else {
        return;
    }

    BaseType_t hpw = pdFALSE;
    (void)xQueueSendFromISR(g_button_q, &btn, &hpw);
    portYIELD_FROM_ISR(hpw);
}
