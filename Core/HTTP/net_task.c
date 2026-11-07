/*
 * net_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "net_task.h"
#include "w5500_port.h"
#include "http_server.h"
#include "usart2.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

#define POLL_MS          10U
#define LINK_CHECK_MS    2000U

void vNetTask(void *pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(500));

    if (W5500_Init() != W5500_OK) {
        (void)USART2_Printf("NET: W5500 init FAILED\r\n");
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    HTTP_Server_Init();

    TickType_t last_wake       = xTaskGetTickCount();
    TickType_t last_link_check = 0U;
    bool       link_was_up     = true;

    for (;;)
    {
        if ((xTaskGetTickCount() - last_link_check) >= pdMS_TO_TICKS(LINK_CHECK_MS)) {
            last_link_check = xTaskGetTickCount();

            uint8_t link = PHY_LINK_OFF;
            (void)ctlwizchip(CW_GET_PHYLINK, &link);
            bool now_up = (link == PHY_LINK_ON);

            if (now_up != link_was_up) {
                (void)USART2_Printf("NET: link %s\r\n", now_up ? "UP" : "DOWN");
                if (!now_up) { (void)close(0U); }
                link_was_up = now_up;
            }
        }

        if (link_was_up) {
            HTTP_Server_Poll();
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(POLL_MS));
    }
}
