/*
 * w5500_test_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "w5500_test_task.h"
#include "w5500_port.h"
#include "wizchip_conf.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"

void vW5500TestTask(void *pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(500));   /* let sensors init first */

    (void)USART2_Printf("W5500: starting init...\r\n");

    W5500_Status_t s = W5500_Init();

    if (s != W5500_OK) {
        (void)USART2_Printf("W5500: init FAILED (code=%d)\r\n", (int)s);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    /* Success — dump actual config to confirm writes stuck */
    const wiz_NetInfo *n = W5500_GetNetInfo();

    uint8_t phy = getPHYCFGR();
    uint8_t mac[6];
    uint8_t ip[4];
    getSHAR(mac);
    getSIPR(ip);

    (void)USART2_Printf("W5500: OK — config applied\r\n");
    (void)USART2_Printf("W5500:   IP   = %u.%u.%u.%u\r\n",
                        (unsigned)ip[0], (unsigned)ip[1],
                        (unsigned)ip[2], (unsigned)ip[3]);
    (void)USART2_Printf("W5500:   MAC  = %02X:%02X:%02X:%02X:%02X:%02X\r\n",
                        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    (void)USART2_Printf("W5500:   PHY  = 0x%02X (lnk=%u spd=%s dpx=%s)\r\n",
                        (unsigned)phy,
                        (unsigned)(phy & 0x01U),
                        ((phy & 0x02U) != 0U) ? "100" : "10",
                        ((phy & 0x04U) != 0U) ? "full" : "half");
    (void)USART2_Printf("W5500:   expect = %u.%u.%u.%u\r\n",
                        (unsigned)n->ip[0], (unsigned)n->ip[1],
                        (unsigned)n->ip[2], (unsigned)n->ip[3]);
    (void)USART2_Printf("W5500: ping 192.168.2.10 from PC now\r\n");

    for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
}
