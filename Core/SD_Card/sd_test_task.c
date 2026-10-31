/*
 * sd_test_task.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "sd_test_task.h"
#include "fatfs.h"          /* USERFatFS, USERPath */
#include "ff.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

void vSdTestTask(void *pv)
{
    (void)pv;

    vTaskDelay(pdMS_TO_TICKS(500));   /* sensors init first */

    /* ---------- 1. Mount ---------- */
    FRESULT res = f_mount(&USERFatFS, USERPath, 1);

    if (res == FR_NO_FILESYSTEM) {
        (void)USART2_Printf("SD: no filesystem, formatting...\r\n");
        (void)f_mount(NULL, USERPath, 1);              /* unmount first */

        /* FatFs R0.13+ requires a work buffer for f_mkfs.
         * 512 bytes is enough for _MAX_SS = 512.
         * static so it doesn't eat task stack. */
        static BYTE work[512];
        res = f_mkfs(USERPath, 0, 0, work, sizeof(work));

        if (res != FR_OK) {
            (void)USART2_Printf("SD: format FAILED (code=%d)\r\n", (int)res);
            for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
        }
        (void)USART2_Printf("SD: format OK, remounting...\r\n");
        res = f_mount(&USERFatFS, USERPath, 1);
    }

    if (res != FR_OK) {
        (void)USART2_Printf("SD: mount FAILED (code=%d)\r\n", (int)res);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }
    (void)USART2_Printf("SD: mount OK\r\n");
    /* ---------- 2. Write test file ---------- */
    FIL fil;
    UINT bw = 0U;
    const char *test_data = "Hello SD card!\r\nLine 2\r\nLine 3\r\n";
    UINT test_len = (UINT)strlen(test_data);

    res = f_open(&fil, "test.txt", FA_CREATE_ALWAYS | FA_WRITE);
    if (res != FR_OK) {
        (void)USART2_Printf("SD: open-for-write FAILED (code=%d)\r\n", (int)res);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    res = f_write(&fil, test_data, test_len, &bw);
    (void)f_close(&fil);   /* flushes buffer to card */

    if ((res != FR_OK) || (bw != test_len)) {
        (void)USART2_Printf("SD: write FAILED (code=%d, wrote %u/%u)\r\n",
                            (int)res, (unsigned)bw, (unsigned)test_len);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }
    (void)USART2_Printf("SD: write OK (%u bytes)\r\n", (unsigned)bw);

    /* ---------- 3. Read back ---------- */
    char buf[64];
    UINT br = 0U;

    res = f_open(&fil, "test.txt", FA_READ);
    if (res != FR_OK) {
        (void)USART2_Printf("SD: open-for-read FAILED (code=%d)\r\n", (int)res);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }

    res = f_read(&fil, buf, sizeof(buf) - 1U, &br);
    (void)f_close(&fil);

    if (res != FR_OK) {
        (void)USART2_Printf("SD: read FAILED (code=%d)\r\n", (int)res);
        for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
    }
    buf[br] = '\0';

    (void)USART2_Printf("SD: read OK (%u bytes)\r\n", (unsigned)br);
    (void)USART2_Printf("SD: content:\r\n---\r\n%s---\r\n", buf);

    if (strcmp(buf, test_data) != 0) {
        (void)USART2_Printf("SD: content MISMATCH\r\n");
    } else {
        (void)USART2_Printf("SD: content VERIFIED — chain works\r\n");
    }

    /* Park */
    for (;;) { vTaskDelay(pdMS_TO_TICKS(10000)); }
}
