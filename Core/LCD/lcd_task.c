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
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define LCD_PERIOD_MS   100U

// Helper
static int32_t clamp_i32(int32_t v, int32_t lo, int32_t hi)
{
    if (v < lo) { return lo; }
    if (v > hi) { return hi; }
    return v;
}

/* Round float × scale to nearest int. */
static int32_t round_scaled(float v, float scale)
{
    float s = v * scale;
    s += (s >= 0.0f) ? 0.5f : -0.5f;
    return (int32_t)s;
}

/* Append C-string. Returns new write pointer. */
static char* put_str(char *p, const char *s)
{
    while (*s != '\0') {
        *p++ = *s++;
    }
    return p;
}

static char* put_scaled(char *p, int32_t scaled, uint8_t decimals)
{
    /* Sign: space for positive, '-' for negative */
    if (scaled < 0) {
        *p++ = '-';
        scaled = -scaled;
    } else {
        *p++ = ' ';
    }

    /* Divisor for the fractional part */
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
        char     tmp[10];
        uint8_t  n = 0U;
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

static void render_temp_hum(const SensorData_t *s, char *l1, char *l2)
{
    if (s->dht_valid) {
        int32_t t10 = clamp_i32(round_scaled(s->temp_c, 10.0f), -999, 999);
        int32_t h10 = clamp_i32(round_scaled(s->hum_pct, 10.0f),    0, 999);

        char *p = l1;
        p = put_str(p, "TEMP:");
        p = put_scaled(p, t10, 1U);
        p = put_str(p, " C");
        *p = '\0';

        p = l2;
        p = put_str(p, "HUMD:");
        p = put_scaled(p, h10, 1U);
        p = put_str(p, " %");
        *p = '\0';
    } else {
        (void)strcpy(l1, "TEMP: --.- C");
        (void)strcpy(l2, "HUMD: --.- %");
    }
}

static void render_accel(const SensorData_t *s, char *l1, char *l2)
{
    if (s->imu_valid) {
        /* 2 decimals */
        int32_t ax = clamp_i32(round_scaled(s->ax_g, 100.0f), -999, 999);
        int32_t ay = clamp_i32(round_scaled(s->ay_g, 100.0f), -999, 999);
        int32_t az = clamp_i32(round_scaled(s->az_g, 100.0f), -999, 999);

        char *p = l1;
        p = put_str(p, "X:");
        p = put_scaled(p, ax, 2U);
        p = put_str(p, " Y:");
        p = put_scaled(p, ay, 2U);
        *p = '\0';

        p = l2;
        p = put_str(p, "Z:");
        p = put_scaled(p, az, 2U);
        p = put_str(p, " g");
        *p = '\0';
    } else {
        (void)strcpy(l1, "X: --.-- Y: --.--");
        (void)strcpy(l2, "Z: --.-- g");
    }
}

static void render_gyro(const SensorData_t *s, char *l1, char *l2)
{
    if (s->imu_valid) {
        /* 0 decimals */
        int32_t gx = clamp_i32((int32_t)s->gx_dps, -999, 999);
        int32_t gy = clamp_i32((int32_t)s->gy_dps, -999, 999);
        int32_t gz = clamp_i32((int32_t)s->gz_dps, -999, 999);

        char *p = l1;
        p = put_str(p, "X:");
        p = put_scaled(p, gx, 0U);
        p = put_str(p, " Y:");
        p = put_scaled(p, gy, 0U);
        *p = '\0';

        p = l2;
        p = put_str(p, "Z:");
        p = put_scaled(p, gz, 0U);
        p = put_str(p, " dps");
        *p = '\0';
    } else {
        (void)strcpy(l1, "X: --- Y: ---");
        (void)strcpy(l2, "Z: --- dps");
    }
}

static void render_datetime(const SensorData_t *s, char *l1, char *l2)
{
    if (s->rtc_valid) {
        char *p = l1;
        p = put_str(p, "20");
        *p++ = (char)('0' + (s->rtc.year  / 10U));
        *p++ = (char)('0' + (s->rtc.year  % 10U));
        *p++ = '-';
        *p++ = (char)('0' + (s->rtc.month / 10U));
        *p++ = (char)('0' + (s->rtc.month % 10U));
        *p++ = '-';
        *p++ = (char)('0' + (s->rtc.date  / 10U));
        *p++ = (char)('0' + (s->rtc.date  % 10U));
        *p = '\0';

        p = l2;
        *p++ = (char)('0' + (s->rtc.hour / 10U));
        *p++ = (char)('0' + (s->rtc.hour % 10U));
        *p++ = ':';
        *p++ = (char)('0' + (s->rtc.min  / 10U));
        *p++ = (char)('0' + (s->rtc.min  % 10U));
        *p++ = ':';
        *p++ = (char)('0' + (s->rtc.sec  / 10U));
        *p++ = (char)('0' + (s->rtc.sec  % 10U));
        *p = '\0';
    } else {
        (void)strcpy(l1, "20--.--.--");
        (void)strcpy(l2, "--:--:--");
    }
}

// LCD write
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

// Task
void vLcdTask(void *pv)
{
    (void) pv;

    if (LCD_Init() != LCD_OK) {
        (void) USART2_Printf("LCD: init failed\r\n");
    }

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
