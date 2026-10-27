/*
 * dwt.c
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#include "dwt.h"
#include "main.h"

#define DWT_CYCCNT_R  (DWT->CYCCNT)

/* Cycles for `us` microseconds. 32-bit wrap-safe under unsigned arithmetic. */
static inline uint32_t us_to_cycles(uint32_t us)
{
    return us * (SystemCoreClock / 1000000U);
}

void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

void DWT_Delay_us(uint32_t us)
{
    uint32_t cycles = us_to_cycles(us);
    uint32_t start  = DWT_CYCCNT_R;
    while ((DWT_CYCCNT_R - start) < cycles) { }
}

void DWT_Delay_ms(uint32_t ms)
{
    while (ms--)
    {
        DWT_Delay_us(1000U);
    }
}

uint32_t DWT_GetCycles(void)
{
    return DWT_CYCCNT_R;
}

bool DWT_Elapsed_us(uint32_t start_cycles, uint32_t us)
{
    /* Unsigned subtraction handles CYCCNT 32-bit wrap (~42.9 s at 100 MHz).
     * Safe for any timeout < one wrap period. */
    return (DWT_CYCCNT_R - start_cycles) > us_to_cycles(us);
}
