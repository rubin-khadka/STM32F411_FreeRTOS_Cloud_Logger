/*
 * sd_spi.h
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#ifndef SD_CARD_SD_SPI_H_
#define SD_CARD_SD_SPI_H_

#include <stdint.h>

typedef enum {
    SD_OK = 0,
    SD_ERROR
} SD_Status;

/* SD card command opcodes */
#define CMD0    0U
#define CMD8    8U
#define CMD12   12U
#define CMD17   17U
#define CMD18   18U
#define CMD24   24U
#define CMD25   25U
#define CMD55   55U
#define CMD58   58U
#define ACMD41  41U

/* Set to 1 after a successful SD_Init() */
extern volatile uint8_t card_initialized;

SD_Status SD_Init(void);
SD_Status SD_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count);
SD_Status SD_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count);
uint8_t   SD_IsSDHC(void);

#endif /* SD_CARD_SD_SPI_H_ */
