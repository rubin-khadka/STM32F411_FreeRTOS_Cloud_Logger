/*
 * usart2.h
 *
 *  Created on: Oct 26, 2026
 *      Author: Rubin Khadka
 */

#ifndef INC_USART2_H_
#define INC_USART2_H_

#include <stdint.h>
#include <stddef.h>

/* Status codes for USART2 */
typedef enum
{
  USART2_OK = 0,
  USART2_ERR_INVALID_ARG,
  USART2_ERR_TIMEOUT,
  USART2_ERR_HAL
} USART2_Status_t;

/* Function Prototypes*/
void USART2_Init(void);
USART2_Status_t USART2_Write(const uint8_t *buf, size_t len);
USART2_Status_t USART2_WriteString(const char *str);
USART2_Status_t USART2_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
USART2_Status_t USART2_Flush(uint32_t timeout_ms);

#endif /* INC_USART2_H_ */
