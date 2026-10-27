/*
 * mpu6050.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "mpu6050.h"
#include "main.h"
#include <string.h>

extern I2C_HandleTypeDef hi2c2;

#define MPU6050_ADDR_8BIT    ((uint16_t)(MPU6050_ADDR_7BIT << 1U))
#define MPU6050_TIMEOUT_MS   (100U)

#define ACCEL_LSB_PER_G      (16384.0f)
#define GYRO_LSB_PER_DPS     (131.0f)
#define TEMP_LSB_PER_C       (340.0f)
#define TEMP_OFFSET_C        (36.53f)
#define INIT_STABILIZE_MS    (100U)

static int16_t mpu_be_to_i16(uint8_t msb, uint8_t lsb)
{
  uint32_t u32 = ((uint32_t) msb << 8U) | (uint32_t) lsb;
  uint16_t u16 = (uint16_t) u32;
  int16_t i16;
  (void) memcpy(&i16, &u16, sizeof(i16));
  return i16;
}

static MPU6050_Status_t mpu_read_regs(uint8_t reg, uint8_t *buf, uint16_t len)
{
  HAL_StatusTypeDef st = HAL_I2C_Mem_Read(&hi2c2,
  MPU6050_ADDR_8BIT, (uint16_t) reg, I2C_MEMADD_SIZE_8BIT, buf, len,
  MPU6050_TIMEOUT_MS);
  return (st == HAL_OK) ? MPU6050_OK : MPU6050_ERR_READ;
}

static MPU6050_Status_t mpu_write_reg(uint8_t reg, uint8_t value)
{
  HAL_StatusTypeDef st = HAL_I2C_Mem_Write(&hi2c2,
  MPU6050_ADDR_8BIT, (uint16_t) reg, I2C_MEMADD_SIZE_8BIT, &value, 1U,
  MPU6050_TIMEOUT_MS);
  return (st == HAL_OK) ? MPU6050_OK : MPU6050_ERR_WRITE;
}

MPU6050_Status_t MPU6050_Init(void)
{
  uint8_t who_am_i = 0U;

  if(mpu_read_regs(MPU6050_REG_WHO_AM_I, &who_am_i, (uint16_t) 1U) != MPU6050_OK)
  {
    return MPU6050_ERR_NOT_FOUND; /* I2C NACK at 0x69 — not present */
  }

  /* 0x68 = genuine MPU6050 */
  if((who_am_i != MPU_ID_MPU6050) && (who_am_i != MPU_ID_CLONE))
  {
    return MPU6050_ERR_NOT_FOUND;
  }

  if(mpu_write_reg(MPU6050_REG_PWR_MGMT_1, 0x00U) != MPU6050_OK)
  {
    return MPU6050_ERR_WRITE;
  }

  HAL_Delay(INIT_STABILIZE_MS);
  return MPU6050_OK;
}

MPU6050_Status_t MPU6050_Read(MPU6050_Data_t *out)
{
  if(out == NULL)
  {
    return MPU6050_ERR_INVALID_ARG;
  }

  uint8_t buf[14U];
  if(mpu_read_regs(MPU6050_REG_ACCEL_XOUT_H, buf, MPU6050_BURST_LEN) != MPU6050_OK)
  {
    return MPU6050_ERR_READ;
  }

  int16_t ax = mpu_be_to_i16(buf[0], buf[1]);
  int16_t ay = mpu_be_to_i16(buf[2], buf[3]);
  int16_t az = mpu_be_to_i16(buf[4], buf[5]);
  int16_t t = mpu_be_to_i16(buf[6], buf[7]);
  int16_t gx = mpu_be_to_i16(buf[8], buf[9]);
  int16_t gy = mpu_be_to_i16(buf[10], buf[11]);
  int16_t gz = mpu_be_to_i16(buf[12], buf[13]);

  out->ax_g = (float) ax / ACCEL_LSB_PER_G;
  out->ay_g = (float) ay / ACCEL_LSB_PER_G;
  out->az_g = (float) az / ACCEL_LSB_PER_G;
  out->gx_dps = (float) gx / GYRO_LSB_PER_DPS;
  out->gy_dps = (float) gy / GYRO_LSB_PER_DPS;
  out->gz_dps = (float) gz / GYRO_LSB_PER_DPS;
  out->temp_c = (float) t / TEMP_LSB_PER_C + TEMP_OFFSET_C;

  return MPU6050_OK;
}
