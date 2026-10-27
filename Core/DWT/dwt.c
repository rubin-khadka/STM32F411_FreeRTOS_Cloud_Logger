/*
 * dwt.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "dwt.h"
#include "main.h"

#define DWT_CYCCNT_R  (DWT->CYCCNT)

void DWT_Init(void)
{
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void DWT_Delay_us(uint32_t us)
{
  uint32_t cycles = (SystemCoreClock / 1000000U) * us;
  uint32_t start = DWT_CYCCNT_R;
  while((DWT_CYCCNT_R - start) < cycles)
  {
  }
}

void DWT_Delay_ms(uint32_t ms)
{
  while(ms--)
  {
    DWT_Delay_us(1000U);
  }
}
