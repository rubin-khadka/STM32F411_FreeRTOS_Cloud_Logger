/*
 * sd_spi.c
 *
 *  Created on: Oct 31, 2026
 *      Author: Rubin Khadka
 */

#include "sd_spi.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

extern SPI_HandleTypeDef hspi1;
#define SD_SPI_HANDLE   hspi1

#define SD_CS_LOW()     HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET)
#define SD_CS_HIGH()    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET)

#define SD_SPI_TIMEOUT_MS   1000U

/* Static dummy TX buffer for SPI reads (0xFF = "read a byte") */
static uint8_t  s_dummy_tx[512];

volatile uint8_t card_initialized = 0U;
static uint8_t   sdhc             = 0U;

uint8_t SD_IsSDHC(void)
{
    return sdhc;
}

/* ---------- SPI primitives ---------- */

static void SD_TransmitByte(uint8_t data)
{
    (void)HAL_SPI_Transmit(&SD_SPI_HANDLE, &data, 1U, SD_SPI_TIMEOUT_MS);
}

static uint8_t SD_ReceiveByte(void)
{
    uint8_t data = 0xFFU;
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, &data, &data, 1U, SD_SPI_TIMEOUT_MS);
    return data;
}

static void SD_TransmitBuffer(const uint8_t *buffer, uint16_t len)
{
    (void)HAL_SPI_Transmit(&SD_SPI_HANDLE, (uint8_t *)buffer, len, SD_SPI_TIMEOUT_MS);
}

static void SD_ReceiveBuffer(uint8_t *buffer, uint16_t len)
{
    (void)HAL_SPI_TransmitReceive(&SD_SPI_HANDLE, s_dummy_tx, buffer, len, SD_SPI_TIMEOUT_MS);
}

/* Wait for card ready (0xFF). Bounded by DWT-based tick timeout. */
static SD_Status SD_WaitReady(void)
{
    TickType_t start   = xTaskGetTickCount();
    TickType_t timeout = pdMS_TO_TICKS(500U);

    do {
        if (SD_ReceiveByte() == 0xFFU) {
            return SD_OK;
        }
    } while ((xTaskGetTickCount() - start) < timeout);

    return SD_ERROR;
}

/* Send one SD command, return R1 response byte. */
static uint8_t SD_SendCommand(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t response;
    uint8_t retry = 0xFFU;

    (void)SD_WaitReady();
    SD_TransmitByte((uint8_t)(0x40U | cmd));
    SD_TransmitByte((uint8_t)(arg >> 24U));
    SD_TransmitByte((uint8_t)(arg >> 16U));
    SD_TransmitByte((uint8_t)(arg >>  8U));
    SD_TransmitByte((uint8_t)(arg));
    SD_TransmitByte(crc);

    do {
        response = SD_ReceiveByte();
    } while (((response & 0x80U) != 0U) && (--retry != 0U));

    return response;
}

/* ---------- Public API ---------- */

SD_Status SD_Init(void)
{
    uint8_t  i;
    uint8_t  response;
    uint8_t  r7[4];
    uint32_t start;
    uint32_t timeout;

    (void)memset(s_dummy_tx, 0xFF, sizeof(s_dummy_tx));

    /* Send 80 dummy clocks with CS high (card power-up sequence) */
    SD_CS_HIGH();
    for (i = 0U; i < 10U; i++) {
        SD_TransmitByte(0xFFU);
    }

    /* CMD0 — go idle */
    SD_CS_LOW();
    response = SD_SendCommand(CMD0, 0U, 0x95U);
    SD_CS_HIGH();
    SD_TransmitByte(0xFFU);
    if (response != 0x01U) {
        return SD_ERROR;
    }

    /* CMD8 — check if card is v2 (SDHC capable) */
    SD_CS_LOW();
    response = SD_SendCommand(CMD8, 0x000001AAU, 0x87U);
    for (i = 0U; i < 4U; i++) {
        r7[i] = SD_ReceiveByte();
    }
    SD_CS_HIGH();
    SD_TransmitByte(0xFFU);

    sdhc    = 0U;
    start   = xTaskGetTickCount();
    timeout = pdMS_TO_TICKS(1000U);

    if ((response == 0x01U) && (r7[2] == 0x01U) && (r7[3] == 0xAAU)) {
        /* SD v2 — ACMD41 with HCS bit for SDHC support */
        do {
            SD_CS_LOW();
            (void)SD_SendCommand(CMD55, 0U, 0xFFU);
            response = SD_SendCommand(ACMD41, 0x40000000U, 0xFFU);
            SD_CS_HIGH();
            SD_TransmitByte(0xFFU);
        } while ((response != 0x00U) && ((xTaskGetTickCount() - start) < timeout));

        if (response != 0x00U) {
            return SD_ERROR;
        }

        /* CMD58 — read OCR, check CCS bit */
        SD_CS_LOW();
        (void)SD_SendCommand(CMD58, 0U, 0xFFU);
        {
            uint8_t ocr[4];
            for (i = 0U; i < 4U; i++) {
                ocr[i] = SD_ReceiveByte();
            }
            if ((ocr[0] & 0x40U) != 0U) {
                sdhc = 1U;
            }
        }
        SD_CS_HIGH();
    } else {
        /* SD v1 / MMC — ACMD41 without HCS */
        do {
            SD_CS_LOW();
            (void)SD_SendCommand(CMD55, 0U, 0xFFU);
            response = SD_SendCommand(ACMD41, 0U, 0xFFU);
            SD_CS_HIGH();
            SD_TransmitByte(0xFFU);
        } while ((response != 0x00U) && ((xTaskGetTickCount() - start) < timeout));

        if (response != 0x00U) {
            return SD_ERROR;
        }
    }

    card_initialized = 1U;
    return SD_OK;
}

SD_Status SD_ReadBlocks(uint8_t *buff, uint32_t sector, uint32_t count)
{
    if ((count == 0U) || (buff == NULL)) {
        return SD_ERROR;
    }

    /* SDSC uses byte address, SDHC uses block address */
    if (sdhc == 0U) {
        sector *= 512U;
    }

    if (count == 1U) {
        SD_CS_LOW();
        if (SD_SendCommand(CMD17, sector, 0xFFU) != 0x00U) {
            SD_CS_HIGH();
            return SD_ERROR;
        }

        /* Wait for data start token */
        uint8_t    token;
        TickType_t start   = xTaskGetTickCount();
        TickType_t timeout = pdMS_TO_TICKS(200U);
        do {
            token = SD_ReceiveByte();
            if (token == 0xFEU) { break; }
        } while ((xTaskGetTickCount() - start) < timeout);

        if (token != 0xFEU) {
            SD_CS_HIGH();
            return SD_ERROR;
        }

        SD_ReceiveBuffer(buff, 512U);
        (void)SD_ReceiveByte();   /* CRC low */
        (void)SD_ReceiveByte();   /* CRC high */

        SD_CS_HIGH();
        SD_TransmitByte(0xFFU);
        return SD_OK;
    }

    /* Multi-block read */
    SD_CS_LOW();
    if (SD_SendCommand(CMD18, sector, 0xFFU) != 0x00U) {
        SD_CS_HIGH();
        return SD_ERROR;
    }

    while (count > 0U) {
        uint8_t    token;
        TickType_t start   = xTaskGetTickCount();
        TickType_t timeout = pdMS_TO_TICKS(200U);

        do {
            token = SD_ReceiveByte();
            if (token == 0xFEU) { break; }
        } while ((xTaskGetTickCount() - start) < timeout);

        if (token != 0xFEU) {
            SD_CS_HIGH();
            return SD_ERROR;
        }

        SD_ReceiveBuffer(buff, 512U);
        (void)SD_ReceiveByte();
        (void)SD_ReceiveByte();

        buff  += 512U;
        count -= 1U;
    }

    (void)SD_SendCommand(CMD12, 0U, 0xFFU);   /* STOP_TRANSMISSION */
    SD_CS_HIGH();
    SD_TransmitByte(0xFFU);

    return SD_OK;
}

SD_Status SD_WriteBlocks(const uint8_t *buff, uint32_t sector, uint32_t count)
{
    if ((count == 0U) || (buff == NULL)) {
        return SD_ERROR;
    }

    if (sdhc == 0U) {
        sector *= 512U;
    }

    if (count == 1U) {
        SD_CS_LOW();
        if (SD_SendCommand(CMD24, sector, 0xFFU) != 0x00U) {
            SD_CS_HIGH();
            return SD_ERROR;
        }

        SD_TransmitByte(0xFEU);              /* data start token */
        SD_TransmitBuffer(buff, 512U);
        (void)SD_ReceiveByte();              /* CRC low — ignored in SPI mode */
        (void)SD_ReceiveByte();              /* CRC high */

        uint8_t resp = SD_ReceiveByte();
        if ((resp & 0x1FU) != 0x05U) {       /* 0x05 = data accepted */
            SD_CS_HIGH();
            return SD_ERROR;
        }

        while (SD_ReceiveByte() == 0x00U) { } /* wait for card busy */

        SD_CS_HIGH();
        SD_TransmitByte(0xFFU);
        return SD_OK;
    }

    /* Multi-block write */
    SD_CS_LOW();
    if (SD_SendCommand(CMD25, sector, 0xFFU) != 0x00U) {
        SD_CS_HIGH();
        return SD_ERROR;
    }

    while (count > 0U) {
        SD_TransmitByte(0xFCU);              /* multi-block write token */
        SD_TransmitBuffer(buff, 512U);
        (void)SD_ReceiveByte();
        (void)SD_ReceiveByte();

        uint8_t resp = SD_ReceiveByte();
        if ((resp & 0x1FU) != 0x05U) {
            SD_CS_HIGH();
            return SD_ERROR;
        }
        while (SD_ReceiveByte() == 0x00U) { }

        buff  += 512U;
        count -= 1U;
    }

    SD_TransmitByte(0xFDU);                  /* STOP_TRAN token */
    while (SD_ReceiveByte() == 0x00U) { }

    SD_CS_HIGH();
    SD_TransmitByte(0xFFU);

    return SD_OK;
}
