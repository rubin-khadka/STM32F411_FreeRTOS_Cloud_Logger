/*
 * dht11.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#ifndef INC_DHT11_H_
#define INC_DHT11_H_

#include <stdint.h>

typedef enum
{
  DHT11_OK = 0,
  DHT11_ERR_INVALID_ARG,
  DHT11_ERR_TIMEOUT,
  DHT11_ERR_NO_RESPONSE,
  DHT11_ERR_CHECKSUM
} DHT11_Status_t;

typedef struct
{
  float temp_c;   // Temperature Celsius
  float hum_pct;  // Humidity Percentage
} DHT11_Data_t;

void DHT11_Init(void); // Initialize DHT11 Sensor
DHT11_Status_t DHT11_ReadFrame(DHT11_Data_t *out); // Full 40-bit read + checksum

#endif /* INC_DHT11_H_ */
