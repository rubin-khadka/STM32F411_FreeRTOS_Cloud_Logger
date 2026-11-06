/*
 * w5500_test_task.c
 *
 *  Created on: Nov 6, 2026
 *      Author: Rubin Khadka
 */

#include "w5500_test_task.h"
#include "w5500_port.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

void vW5500TestTask(void *pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(500));   /* let sensors init */

    /* Retry ONLY on failure — never re-init after success */
    while (W5500_Init() != W5500_OK) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    const wiz_NetInfo *n = W5500_GetNetInfo();
    (void)USART2_Printf("W5500: up at %u.%u.%u.%u\r\n",
                        (unsigned)n->ip[0], (unsigned)n->ip[1],
                        (unsigned)n->ip[2], (unsigned)n->ip[3]);

    /* Idle — chip stays running, no more resets */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
