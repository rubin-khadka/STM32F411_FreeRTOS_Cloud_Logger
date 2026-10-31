/*
 * button_task.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "button_task.h"
#include "button.h"
#include "usart2.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stdbool.h"
#include "queue.h"

#define COOLDOWN_BTN1_MS    0U     /* no cooldown */
#define COOLDOWN_BTN2_MS    500U   /* prevent UART spam */
#define DEBOUNCE_MS         20U

static bool button_is_pressed(uint8_t btn)
{
    switch (btn) {
        case 1U: return HAL_GPIO_ReadPin(BTN1_GPIO_Port, BTN1_Pin) == GPIO_PIN_RESET;
        case 2U: return HAL_GPIO_ReadPin(BTN2_GPIO_Port, BTN2_Pin) == GPIO_PIN_RESET;
        default: return false;
    }
}

static void print_snapshot(void)
{
    (void) USART2_Printf("[BTN2] snapshot requested (sensor publishing pending)\r\n");
}

static void drain_bounce_events(uint8_t btn)
{
    uint8_t d;
    while ((xQueuePeek(g_button_q, &d, 0U) == pdTRUE) && (d == btn)) {
        (void) xQueueReceive(g_button_q, &d, 0U);
    }
}

void vButtonTask(void *pv)
{
    (void) pv;

    TickType_t last_press[2] = {0U, 0U};
    uint8_t    btn;

    for (;;)
    {
        if (xQueueReceive(g_button_q, &btn, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if ((btn < 1U) || (btn > 2U)) {
            continue;
        }

        /* --- Debounce: wait 20 ms, re-read pin --- */
        vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_MS));
        if (!button_is_pressed(btn)) {
            continue;   /* bounce or glitch */
        }

        /* --- Cooldown check --- */
        TickType_t now = xTaskGetTickCount();
        TickType_t cd  = (btn == 1U) ? pdMS_TO_TICKS(COOLDOWN_BTN1_MS)
                                     : pdMS_TO_TICKS(COOLDOWN_BTN2_MS);

        if ((cd != 0U) &&
            (last_press[btn - 1U] != 0U) &&
            ((now - last_press[btn - 1U]) < cd))
        {
            drain_bounce_events(btn);
            continue;
        }
        last_press[btn - 1U] = now;

        /* --- Dispatch --- */
        if (btn == 1U) {
            Button_NextMode();
            (void) USART2_Printf("[BTN1] mode → %d\r\n", (int)Button_GetMode());
        } else {
            print_snapshot();
        }

        drain_bounce_events(btn);
    }
}
