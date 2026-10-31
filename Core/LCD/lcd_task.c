/*
 * lcd_task.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "lcd_task.h"
#include "lcd.h"
#include "button.h"
#include "sensor_data.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

#define LCD_PERIOD_MS   100U

/* Clamp int32 to [lo, hi] — keeps snprintf output predictable. */
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

/* Split signed integer into sign char + magnitude, clamped.
 * Result fits in `max_digits` digits for the integer part. */
static void fmt_scaled(int32_t scaled, int32_t max_abs,
                       char *sign, int32_t *mag)
{
    if (scaled < 0) { *sign = '-'; *mag = -scaled; }
    else            { *sign = ' '; *mag =  scaled; }
    if (*mag > max_abs) *mag = max_abs;
}

static void render_temp_hum(const SensorData_t *s, char *l1, char *l2)
{
    if (s->dht_valid) {
        int32_t t10 = (int32_t)(s->temp_c * 10.0f);
        int32_t h10 = (int32_t)(s->hum_pct * 10.0f);
        char    ts, hs;
        int32_t tm, hm;

        /* temp: clamp to -99.9..99.9 → 3 int digits, 1 frac */
        fmt_scaled(t10, 999, &ts, &tm);
        /* hum: clamp to 99.9 → 2 int digits, 1 frac */
        fmt_scaled(h10, 999, &hs, &hm);

        (void)snprintf(l1, 17, "TEMP:%c%ld.%ld C",
                       ts, (long)(tm / 10), (long)(tm % 10));
        (void)snprintf(l2, 17, "HUMD:%c%ld.%ld %%",
                       hs, (long)(hm / 10), (long)(hm % 10));
    } else {
        (void)snprintf(l1, 17, "TEMP: --.- C");
        (void)snprintf(l2, 17, "HUMD: --.- %%");
    }
}

static void render_accel(const SensorData_t *s, char *l1, char *l2)
{
    if (s->imu_valid) {
        /* accel: clamp to ±9.99 g → 1 int digit, 2 frac, fits in 6 chars */
        int32_t ax = (int32_t)(s->ax_g * 100.0f);
        int32_t ay = (int32_t)(s->ay_g * 100.0f);
        int32_t az = (int32_t)(s->az_g * 100.0f);
        char    axs, ays, azs;
        int32_t axm, aym, azm;

        fmt_scaled(ax, 999, &axs, &axm);   /* max 9.99 */
        fmt_scaled(ay, 999, &ays, &aym);
        fmt_scaled(az, 999, &azs, &azm);

        (void)snprintf(l1, 17, "X:%c%ld.%02ld Y:%c%ld.%02ld",
                       axs, (long)(axm / 100), (long)(axm % 100),
                       ays, (long)(aym / 100), (long)(aym % 100));
        (void)snprintf(l2, 17, "Z:%c%ld.%02ld g",
                       azs, (long)(azm / 100), (long)(azm % 100));
    } else {
        (void)snprintf(l1, 17, "X:--.-- Y:--.--");
        (void)snprintf(l2, 17, "Z:--.-- g");
    }
}

static void render_gyro(const SensorData_t *s, char *l1, char *l2)
{
    if (s->imu_valid) {
        /* gyro: clamp to ±999.9 dps → 3 int digits, 1 frac */
        int32_t gx = (int32_t)(s->gx_dps * 10.0f);
        int32_t gy = (int32_t)(s->gy_dps * 10.0f);
        int32_t gz = (int32_t)(s->gz_dps * 10.0f);
        char    gxs, gys, gzs;
        int32_t gxm, gym, gzm;

        fmt_scaled(gx, 9999, &gxs, &gxm);  /* max 999.9 */
        fmt_scaled(gy, 9999, &gys, &gym);
        fmt_scaled(gz, 9999, &gzs, &gzm);

        (void)snprintf(l1, 17, "X:%c%ld.%ld Y:%c%ld.%ld",
                       gxs, (long)(gxm / 10), (long)(gxm % 10),
                       gys, (long)(gym / 10), (long)(gym % 10));
        (void)snprintf(l2, 17, "Z:%c%ld.%ld dps",
                       gzs, (long)(gzm / 10), (long)(gzm % 10));
    } else {
        (void)snprintf(l1, 17, "X:--.- Y:--.-");
        (void)snprintf(l2, 17, "Z:--.- dps");
    }
}

static void render_datetime(const SensorData_t *s, char *l1, char *l2)
{
    if (s->rtc_valid) {
        (void)snprintf(l1, 17, "20%02d-%02d-%02d",
                       (int)s->rtc.year, (int)s->rtc.month, (int)s->rtc.date);
        (void)snprintf(l2, 17, "%02d:%02d:%02d",
                       (int)s->rtc.hour, (int)s->rtc.min, (int)s->rtc.sec);
    } else {
        (void)snprintf(l1, 17, "20--.--.--");
        (void)snprintf(l2, 17, "--:--:--");
    }
}

static void lcd_write_line(uint8_t row, const char *text)
{
    LCD_SetCursor(row, 0U);
    uint8_t i = 0U;
    while ((i < 16U) && (text[i] != '\0')) {
        LCD_SendChar(text[i]);
        i++;
    }
    while (i < 16U) {
        LCD_SendChar(' ');
        i++;
    }
}

void vLcdTask(void *pv)
{
    (void) pv;

    LCD_Init();

    TickType_t   last_wake = xTaskGetTickCount();
    SensorData_t snap;
    char         line1[17];
    char         line2[17];
    char         last_line1[17] = { 0 };
    char         last_line2[17] = { 0 };

    for (;;)
    {
        SensorData_GetSnapshot(&snap);

        switch (Button_GetMode())
        {
            case DISPLAY_MODE_TEMP_HUM: render_temp_hum(&snap, line1, line2); break;
            case DISPLAY_MODE_ACCEL:    render_accel   (&snap, line1, line2); break;
            case DISPLAY_MODE_GYRO:     render_gyro    (&snap, line1, line2); break;
            case DISPLAY_MODE_DATETIME:
            default:                    render_datetime(&snap, line1, line2); break;
        }

        if (strcmp(line1, last_line1) != 0) {
            lcd_write_line(0U, line1);
            (void)strcpy(last_line1, line1);
        }
        if (strcmp(line2, last_line2) != 0) {
            lcd_write_line(1U, line2);
            (void)strcpy(last_line2, line2);
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(LCD_PERIOD_MS));
    }
}
