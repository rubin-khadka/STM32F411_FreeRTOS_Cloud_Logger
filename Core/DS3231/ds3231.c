/*
 * ds3231.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "ds3231.h"
#include "main.h"

extern I2C_HandleTypeDef hi2c2;

#define DS3231_ADDR_8BIT     ((uint16_t)(DS3231_ADDR_7BIT << 1U))
#define DS3231_TIMEOUT_MS    (100U)

/* --- BCD helpers (private) --- */

static uint8_t bcd_to_bin(uint8_t bcd)
{
  uint8_t tens = (uint8_t) ((bcd >> 4U) & 0x0FU);
  uint8_t ones = (uint8_t) (bcd & 0x0FU);
  return (uint8_t) ((tens * 10U) + ones);
}

static uint8_t bin_to_bcd(uint8_t bin)
{
  uint8_t tens = (uint8_t) (bin / 10U);
  uint8_t ones = (uint8_t) (bin % 10U);
  return (uint8_t) ((tens << 4U) | ones);
}

/* --- I2C helpers (private) --- */

static DS3231_Status_t ds_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
  HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2,
  DS3231_ADDR_8BIT, (uint16_t) reg,
  I2C_MEMADD_SIZE_8BIT, buf, len,
  DS3231_TIMEOUT_MS);
  return (st == HAL_OK) ? DS3231_OK : DS3231_ERR_READ;
}

static DS3231_Status_t ds_write_regs(uint8_t reg, const uint8_t *buf, uint16_t len)
{
  HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c2,
  DS3231_ADDR_8BIT, (uint16_t) reg,
  I2C_MEMADD_SIZE_8BIT, (uint8_t*) buf, len,
  DS3231_TIMEOUT_MS);
  return (st == HAL_OK) ? DS3231_OK : DS3231_ERR_WRITE;
}

/* --- Public API --- */

DS3231_Status_t DS3231_Init(void)
{
  uint8_t status = 0U;

  DS3231_Status_t s = ds_read_regs(DS3231_REG_STATUS, &status, (uint16_t) 1U);
  if(s != DS3231_OK)
  {
    return DS3231_ERR_NOT_FOUND; /* I2C NACK — no device at 0x68 */
  }

  if((status & DS3231_STATUS_OSF) != 0U)
  {
    return DS3231_ERR_TIME_INVALID; /* time was lost — caller must SetTime */
  }

  return DS3231_OK;
}

DS3231_Status_t DS3231_ReadTime(DS3231_Time_t *out)
{
  if(out == NULL)
  {
    return DS3231_ERR_INVALID_ARG;
  }

  uint8_t buf[7];
  DS3231_Status_t s = ds_read_regs(DS3231_REG_SEC, buf, DS3231_TIME_BURST_LEN);
  if(s != DS3231_OK)
  {
    return s;
  }

  /* Decode BCD, masking control bits.
   * Mask list:
   *   seconds: bit 7 = CH (clock halt) — not used on DS3231, mask anyway
   *   minutes: bit 7 = 0
   *   hours:   bit 6 = 12/24 mode, bit 5 = AM/PM in 12h mode; assume 24h → mask 0x3F
   *   dow:     bits 7–3 = 0
   *   date:    bits 7–6 = 0
   *   month:   bit 7 = century, bit 6 = 0
   *   year:    full byte is valid BCD */
  out->sec = bcd_to_bin((uint8_t) (buf[0] & 0x7FU));
  out->min = bcd_to_bin((uint8_t) (buf[1] & 0x7FU));
  out->hour = bcd_to_bin((uint8_t) (buf[2] & 0x3FU));
  out->dow = bcd_to_bin((uint8_t) (buf[3] & 0x07U));
  out->date = bcd_to_bin((uint8_t) (buf[4] & 0x3FU));
  out->month = bcd_to_bin((uint8_t) (buf[5] & 0x1FU));
  out->year = bcd_to_bin(buf[6]);

  return DS3231_OK;
}

DS3231_Status_t DS3231_SetTime(const DS3231_Time_t *in)
{
  if(in == NULL)
  {
    return DS3231_ERR_INVALID_ARG;
  }

  /* Range check — reject anything obviously invalid before writing */
  if((in->sec > 59U) || (in->min > 59U) || (in->hour > 23U) || (in->dow < 1U) || (in->dow > 7U) || (in->date < 1U)
      || (in->date > 31U) || (in->month < 1U) || (in->month > 12U) || (in->year > 99U))
  {
    return DS3231_ERR_INVALID_ARG;
  }

  uint8_t buf[7];
  buf[0] = bin_to_bcd(in->sec);
  buf[1] = bin_to_bcd(in->min);
  buf[2] = bin_to_bcd(in->hour); /* bit 6 = 0 → 24-hour mode */
  buf[3] = bin_to_bcd(in->dow);
  buf[4] = bin_to_bcd(in->date);
  buf[5] = bin_to_bcd(in->month); /* bit 7 = 0 → 2000–2099 */
  buf[6] = bin_to_bcd(in->year);

  DS3231_Status_t s = ds_write_regs(DS3231_REG_SEC, buf, DS3231_TIME_BURST_LEN);
  if(s != DS3231_OK)
  {
    return s;
  }

  /* Clear OSF to mark time as valid again */
  uint8_t status = 0U;
  s = ds_read_regs(DS3231_REG_STATUS, &status, (uint16_t) 1U);
  if(s != DS3231_OK)
  {
    return s;
  }

  status &= (uint8_t) (~DS3231_STATUS_OSF);
  s = ds_write_regs(DS3231_REG_STATUS, &status, (uint16_t) 1U);

  return s;
}

DS3231_Status_t DS3231_ReadTemperature(float *out_c)
{
  if(out_c == NULL)
  {
    return DS3231_ERR_INVALID_ARG;
  }

  /* Single burst read of MSB + LSB — atomic, no race with the 64 s
   * internal conversion cycle. */
  uint8_t buf[2];
  DS3231_Status_t s = ds_read_regs(DS3231_REG_TEMP_MSB, buf, DS3231_TEMP_BURST_LEN);
  if(s != DS3231_OK)
  {
    return s;
  }

  /* Format: MSB = signed integer part, LSB[7:6] = fractional × 0.25 °C.
   * Example: MSB = -6 (0xFA), LSB[7:6] = 0b01 → -6 + 0.25 = -5.75 °C */
  int8_t int_part = (int8_t) buf[0];
  uint8_t frac_q = (uint8_t) (((uint32_t) buf[1] >> 6U) & 0x03U); /* 0..3 */
  *out_c = (float) int_part + ((float) frac_q * 0.25f);

  return DS3231_OK;
}
