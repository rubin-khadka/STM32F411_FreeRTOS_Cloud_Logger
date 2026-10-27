/*
 * dwt.h
 *
 *  Created on: Oct 27, 2026
 *      Author: Rubin Khadka
 */

#ifndef INC_DWT_H_
#define INC_DWT_H_

#include <stdint.h>
#include <stdbool.h>

void DWT_Init(void);
void DWT_Delay_us(uint32_t us);
void DWT_Delay_ms(uint32_t ms);

uint32_t DWT_GetCycles(void);
bool DWT_Elapsed_us(uint32_t start_cycles, uint32_t us);

#endif /* INC_DWT_H_ */
