/*
 * sd_task.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "sd_task.h"
#include "fatfs.h"
#include "ff.h"
#include "sensor_data.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define SD_LOG_PERIOD_MS    10000U     /* 10 s */
#define CSV_FILENAME        "senData.csv"
#define CSV_HEADER          "DateTime,DHT11_C,DHT11_%,AX_g,AY_g,AZ_g,GX_dps,GY_dps,GZ_dps\r\n"

static char* put_str(char *p, const char *s)
{
    while (*s != '\0') {
        *p++ = *s++;
    }
    return p;
}

static char* put_2digit(char *p, uint8_t v)
{
    *p++ = (char)('0' + (v / 10U));
    *p++ = (char)('0' + (v % 10U));
    return p;
}

static char* put_scaled(char *p, int32_t scaled, uint8_t decimals)
{
    if (scaled < 0) {
        *p++ = '-';
        scaled = -scaled;
    }

    uint32_t div = 1U;
    for (uint8_t i = 0U; i < decimals; i++) {
        div *= 10U;
    }

    uint32_t int_part  = (uint32_t)scaled / div;
    uint32_t frac_part = (uint32_t)scaled % div;

    /* Integer part */
    if (int_part == 0U) {
        *p++ = '0';
    } else {
        char    tmp[10];
        uint8_t n = 0U;
        while (int_part > 0U) {
            tmp[n++] = (char)('0' + (int_part % 10U));
            int_part /= 10U;
        }
        while (n > 0U) {
            *p++ = tmp[--n];
        }
    }

    /* Fractional part */
    if (decimals > 0U) {
        *p++ = '.';
        div /= 10U;
        while (div > 0U) {
            *p++ = (char)('0' + (frac_part / div));
            frac_part %= div;
            div /= 10U;
        }
    }

    return p;
}

/* Round float × scale to nearest int. */
static int32_t round_scaled(float v, float scale)
{
    float s = v * scale;
    s += (s >= 0.0f) ? 0.5f : -0.5f;
    return (int32_t)s;
}

static void build_csv_row(char *row, const SensorData_t *s)
{
    char *p = row;

    /* Timestamp: 20YY-MM-DD HH:MM:SS */
    p = put_str(p, "20");
    p = put_2digit(p, s->rtc.year);
    *p++ = '-';
    p = put_2digit(p, s->rtc.month);
    *p++ = '-';
    p = put_2digit(p, s->rtc.date);
    *p++ = ' ';
    p = put_2digit(p, s->rtc.hour);
    *p++ = ':';
    p = put_2digit(p, s->rtc.min);
    *p++ = ':';
    p = put_2digit(p, s->rtc.sec);

    /* DHT11 temperature — 1 decimal */
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->temp_c, 10.0f), 1U);

    /* DHT11 humidity — 1 decimal */
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->hum_pct, 10.0f), 1U);

    /* Accelerometer — 3 decimals */
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->ax_g, 1000.0f), 3U);
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->ay_g, 1000.0f), 3U);
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->az_g, 1000.0f), 3U);

    /* Gyroscope — 2 decimals */
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->gx_dps, 100.0f), 2U);
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->gy_dps, 100.0f), 2U);
    *p++ = ',';
    p = put_scaled(p, round_scaled(s->gz_dps, 100.0f), 2U);

    /* End line */
    *p++ = '\r';
    *p++ = '\n';
    *p = '\0';
}


// Task
void vSdTask(void *pv)
{
    (void)pv;

    /* --- Mount (auto-format if blank) --- */
    FRESULT res = f_mount(&USERFatFS, USERPath, 1);

    if (res == FR_NO_FILESYSTEM) {
        (void)f_mount(NULL, USERPath, 1);
        static BYTE work[512];
        res = f_mkfs(USERPath, 0, 0, work, sizeof(work));
        if (res == FR_OK) {
            res = f_mount(&USERFatFS, USERPath, 1);
        }
    }

    if (res != FR_OK) {
        (void)USART2_Printf("SD: mount failed (code=%d) — task parked\r\n",
                            (int)res);
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    }

    /* --- Ensure CSV file exists with header --- */
    {
        FIL fil;
        UINT bw = 0U;
        if (f_open(&fil, CSV_FILENAME, FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
            if (f_size(&fil) == 0U) {
                (void)f_write(&fil, CSV_HEADER,
                              (UINT)strlen(CSV_HEADER), &bw);
            }
            (void)f_close(&fil);
        }
    }

    /* --- Main loop: 10 s logging --- */
    TickType_t   last_wake = xTaskGetTickCount();
    SensorData_t snap;

    for (;;) {
        SensorData_GetSnapshot(&snap);

        if (snap.dht_valid && snap.imu_valid && snap.rtc_valid) {
            char row[128];
            build_csv_row(row, &snap);

            FIL  fil;
            UINT wrote = 0U;
            size_t len = strlen(row);

            FRESULT wr = f_open(&fil, CSV_FILENAME, FA_OPEN_APPEND | FA_WRITE);
            if (wr == FR_OK) {
                wr = f_write(&fil, row, (UINT)len, &wrote);
                (void)f_close(&fil);

                if ((wr != FR_OK) || (wrote != (UINT)len)) {
                    (void)USART2_Printf("SD: write failed (code=%d)\r\n",
                                        (int)wr);
                }
                /* else: silent success */
            } else {
                (void)USART2_Printf("SD: open failed (code=%d)\r\n",
                                    (int)wr);
            }
        }
        /* If any sensor hasn't reported yet, skip this cycle silently */

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(SD_LOG_PERIOD_MS));
    }
}
