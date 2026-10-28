/*
 * ds3231.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#ifndef DS3231_DS3231_H_
#define DS3231_DS3231_H_

#include <stdint.h>

/* 7-bit I2C address */
#define DS3231_ADDR_7BIT     (0x68U)

/* Register addresses */
#define DS3231_REG_SEC       (0x00U)
#define DS3231_REG_MIN       (0x01U)
#define DS3231_REG_HOUR      (0x02U)
#define DS3231_REG_DOW       (0x03U)
#define DS3231_REG_DATE      (0x04U)
#define DS3231_REG_MONTH     (0x05U)
#define DS3231_REG_YEAR      (0x06U)
#define DS3231_REG_STATUS    (0x0FU)
#define DS3231_REG_TEMP_MSB  (0x11U)
#define DS3231_REG_TEMP_LSB  (0x12U)

/* Burst lengths */
#define DS3231_TIME_BURST_LEN  ((uint16_t) 7U)   /* seconds..year */
#define DS3231_TEMP_BURST_LEN  ((uint16_t) 2U)   /* MSB + LSB */

/* Status register: Oscillator Stop Flag (bit 7). */
#define DS3231_STATUS_OSF    (0x80U)

typedef enum {
    DS3231_OK = 0,
    DS3231_ERR_INVALID_ARG,
    DS3231_ERR_NOT_FOUND,      /* I2C NACK at 0x68 */
    DS3231_ERR_READ,
    DS3231_ERR_WRITE,
    DS3231_ERR_TIME_INVALID    /* OSF set — time was lost, must call SetTime() */
} DS3231_Status_t;

/* All fields decoded from BCD to plain binary. */
typedef struct {
    uint8_t sec;      /* 0–59 */
    uint8_t min;      /* 0–59 */
    uint8_t hour;     /* 0–23 (24-hour mode) */
    uint8_t dow;      /* 1–7 */
    uint8_t date;     /* 1–31 */
    uint8_t month;    /* 1–12 */
    uint8_t year;     /* 0–99 → 2000–2099 */
} DS3231_Time_t;

DS3231_Status_t DS3231_Init(void);                                /* returns ERR_TIME_INVALID if OSF set */
DS3231_Status_t DS3231_ReadTime(DS3231_Time_t *out);
DS3231_Status_t DS3231_SetTime(const DS3231_Time_t *in);          /* writes time + clears OSF */
DS3231_Status_t DS3231_ReadTemperature(float *out_c);

#endif /* DS3231_DS3231_H_ */
