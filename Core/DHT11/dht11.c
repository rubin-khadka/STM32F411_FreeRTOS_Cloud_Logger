/*
 * dht11.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "dht11.h"
#include "main.h"
#include "dwt.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdbool.h>

/* Pin — must match CubeMX */
#define DHT_PORT  GPIOB
#define DHT_PIN   GPIO_PIN_0

/* Protocol timings */
#define DHT_START_PULSE_MS       18U
#define DHT_TIMEOUT_RESPONSE_US  200U
#define DHT_TIMEOUT_BIT_US       200U
#define DHT_SAMPLE_DELAY_US      40U

static inline uint32_t us_to_ticks(uint32_t us)
{
  return us * (SystemCoreClock / 1000000U);
}

void DHT11_Init(void)
{
  DWT_Init();

  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET); /* idle HIGH via pull-up */
}

/* Wait for pin to reach `level` */
static DHT11_Status_t dht_wait_level(GPIO_PinState level, uint32_t timeout_us)
{
  uint32_t start = DWT->CYCCNT;
  uint32_t ticks = us_to_ticks(timeout_us);

  while(HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) != level)
  {
    if((DWT->CYCCNT - start) > ticks)
    {
      return DHT11_ERR_TIMEOUT;
    }
  }
  return DHT11_OK;
}

/* Sensor response: 80 µs LOW, 80 µs HIGH, then falling edge. */
static DHT11_Status_t dht11_wait_response(void)
{
  if(dht_wait_level(GPIO_PIN_RESET, DHT_TIMEOUT_RESPONSE_US) != DHT11_OK)
  {
    return DHT11_ERR_NO_RESPONSE;
  }
  if(dht_wait_level(GPIO_PIN_SET, DHT_TIMEOUT_RESPONSE_US) != DHT11_OK)
  {
    return DHT11_ERR_NO_RESPONSE;
  }
  if(dht_wait_level(GPIO_PIN_RESET, DHT_TIMEOUT_RESPONSE_US) != DHT11_OK)
  {
    return DHT11_ERR_NO_RESPONSE;
  }
  return DHT11_OK;
}

/* Read 8 bits. */
static DHT11_Status_t dht11_read_byte(uint8_t *out, bool is_last)
{
  uint8_t data = 0;

  for(int i = 7; i >= 0; i--)
  {
    /* Rising edge: LOW separator ends, bit's HIGH phase starts. */
    if(dht_wait_level(GPIO_PIN_SET, DHT_TIMEOUT_BIT_US) != DHT11_OK)
    {
      return DHT11_ERR_TIMEOUT;
    }

    /* Sample at 40 µs:
     *   still HIGH → bit 1 (70 µs phase ongoing)
     *   already LOW → bit 0 (26 µs phase ended) */
    DWT_Delay_us(DHT_SAMPLE_DELAY_US);

    if(HAL_GPIO_ReadPin(DHT_PORT, DHT_PIN) == GPIO_PIN_SET)
    {
      data |= (uint8_t) (1U << i);

      bool last_bit_of_frame = is_last && (i == 0);
      if(!last_bit_of_frame)
      {
        if(dht_wait_level(GPIO_PIN_RESET, DHT_TIMEOUT_BIT_US) != DHT11_OK)
        {
          return DHT11_ERR_TIMEOUT;
        }
      }
    }
    /* If bit = 0, line is already LOW — next iteration handles it. */
  }

  *out = data;
  return DHT11_OK;
}

DHT11_Status_t DHT11_ReadFrame(DHT11_Data_t *out)
{
  if(out == NULL)
  {
    return DHT11_ERR_INVALID_ARG;
  }

  uint8_t hum_i = 0, hum_d = 0, tmp_i = 0, tmp_d = 0, chk = 0;
  DHT11_Status_t s;

  /* 1. Start pulse: drive LOW 18 ms, then release.
   *    vTaskDelay yields to other tasks — no CPU burn. */
  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_RESET);
  vTaskDelay(pdMS_TO_TICKS(DHT_START_PULSE_MS));
  HAL_GPIO_WritePin(DHT_PORT, DHT_PIN, GPIO_PIN_SET);

  /* 2. Response + 40 bits, ~4.5 ms busy-wait under critical section.
   *    Masks SysTick and all peripheral IRQs (priority ≥ configMAX_SYSCALL). */
  taskENTER_CRITICAL();

  s = dht11_wait_response();
  if(s == DHT11_OK)
  {
    s = dht11_read_byte(&hum_i, false);
  }
  if(s == DHT11_OK)
  {
    s = dht11_read_byte(&hum_d, false);
  }
  if(s == DHT11_OK)
  {
    s = dht11_read_byte(&tmp_i, false);
  }
  if(s == DHT11_OK)
  {
    s = dht11_read_byte(&tmp_d, false);
  }
  if(s == DHT11_OK)
  {
    s = dht11_read_byte(&chk, true);
  }

  taskEXIT_CRITICAL();

  /* 3. Error propagation — outside critical section. */
  if(s != DHT11_OK)
  {
    return s;
  }

  /* 4. Checksum: sum of first 4 bytes mod 256 must equal byte 5. */
  uint8_t sum = (uint8_t) (hum_i + hum_d + tmp_i + tmp_d);
  if(sum != chk)
  {
    return DHT11_ERR_CHECKSUM;
  }

  /* 5. Convert to floats. Decimal bytes are tenths. */
  out->hum_pct = (float) hum_i + ((float) hum_d / 10.0f);
  out->temp_c = (float) tmp_i + ((float) tmp_d / 10.0f);

  return DHT11_OK;
}
