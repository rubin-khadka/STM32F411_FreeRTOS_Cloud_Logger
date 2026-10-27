/*
 * mpu6050.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#ifndef MPU6050_MPU6050_H_
#define MPU6050_MPU6050_H_

#include <stdint.h>

/* 7-bit I2C address: AD0 connected HIGH */
#define MPU6050_ADDR_7BIT         (0x69U)

/* Register addresses */
#define MPU6050_REG_WHO_AM_I      (0x75U)
#define MPU6050_REG_PWR_MGMT_1    (0x6BU)
#define MPU6050_REG_ACCEL_XOUT_H  (0x3BU)   /* burst start: 14 bytes */

/* Burst length (accel 6 + temp 2 + gyro 6) */
#define MPU6050_BURST_LEN         ((uint16_t) 14U)

/* Expected WHO_AM_I values, cast to uint8_t so comparisons stay */
#define MPU_ID_MPU6050            ((uint8_t) 0x68U)
#define MPU_ID_CLONE              ((uint8_t) 0x69U)

typedef enum {
    MPU6050_OK = 0,
    MPU6050_ERR_INVALID_ARG,
    MPU6050_ERR_NOT_FOUND,
    MPU6050_ERR_READ,
    MPU6050_ERR_WRITE
} MPU6050_Status_t;

typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float temp_c;
} MPU6050_Data_t;

MPU6050_Status_t MPU6050_Init(void);
MPU6050_Status_t MPU6050_Read(MPU6050_Data_t *out);

#endif /* MPU6050_MPU6050_H_ */
