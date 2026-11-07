/*
 * w5500_port.c
 *
 *  Created on: Nov 6, 2026
 *      Author: Rubin Khadka
 */

#include "w5500_port.h"
#include "main.h"
#include "wizchip_conf.h"
#include "socket.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usart2.h"

/* ---------- Chip connection ---------- */

extern SPI_HandleTypeDef hspi2;
#define W5500_SPI        hspi2

#define W5500_CS_LOW()   HAL_GPIO_WritePin(CS_GPIO_Port,    CS_Pin,    GPIO_PIN_RESET)
#define W5500_CS_HIGH()  HAL_GPIO_WritePin(CS_GPIO_Port,    CS_Pin,    GPIO_PIN_SET)
#define W5500_RST_LOW()  HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_RESET)
#define W5500_RST_HIGH() HAL_GPIO_WritePin(RESET_GPIO_Port, RESET_Pin, GPIO_PIN_SET)

#define W5500_SPI_TIMEOUT_MS   1000U

#define W5500_LINK_RETRIES     10U
#define W5500_LINK_POLL_MS     500U

/* ---------- Network configuration (pure static) ---------- */

static wiz_NetInfo s_net_info = { .mac = { 0x02, 0x00, 0x00, 0x00, 0x00, 0x01 }, .ip = { 192, 168, 2, 10 }, .sn =
    { 255, 255, 255, 0 }, .gw = { 0, 0, 0, 0 }, .dns = { 0, 0, 0, 0 }, .dhcp = NETINFO_STATIC };

/* ---------- SPI callbacks ---------- */

static void W5500_Select(void)
{
  W5500_CS_LOW();
}

static void W5500_Unselect(void)
{
  W5500_CS_HIGH();
}

static uint8_t W5500_ReadByte(void)
{
  uint8_t rx = 0xFFU;
  uint8_t tx = 0xFFU;
  (void) HAL_SPI_TransmitReceive(&W5500_SPI, &tx, &rx, 1U, W5500_SPI_TIMEOUT_MS);
  return rx;
}

static void W5500_WriteByte(uint8_t byte)
{
  (void) HAL_SPI_Transmit(&W5500_SPI, &byte, 1U, W5500_SPI_TIMEOUT_MS);
}

/* ---------- Init ---------- */

W5500_Status_t W5500_Init(void)
{
  uint8_t memsize[2][8] = { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 2, 2, 2, 2, 2, 2, 2 } };

  /* --- Hard reset --- */
  W5500_RST_LOW();
  vTaskDelay(pdMS_TO_TICKS(50));
  W5500_RST_HIGH();
  vTaskDelay(pdMS_TO_TICKS(200));

  /* --- Register ioLibrary callbacks --- */
  reg_wizchip_cs_cbfunc(W5500_Select, W5500_Unselect);
  reg_wizchip_spi_cbfunc(W5500_ReadByte, W5500_WriteByte);

  /* --- Chip init --- */
  if(ctlwizchip(CW_INIT_WIZCHIP, (void*) memsize) == -1)
  {
    (void) USART2_Printf("W5500: chip init FAILED\r\n");
    return W5500_ERR_INIT;
  }

  /* --- Version check (expect 0x04) --- */
  uint8_t ver = getVERSIONR();
  if(ver != 0x04U)
  {
    (void) USART2_Printf("W5500: version FAILED (got 0x%02X, expect 0x04)\r\n", (unsigned) ver);
    return W5500_ERR_COMM;
  }

  /* --- Link wait (10 × 500 ms = 5 s max) --- */
  uint8_t link = PHY_LINK_OFF;
  uint8_t retries = W5500_LINK_RETRIES;

  while((link != PHY_LINK_ON) && (retries > 0U))
  {
    (void) ctlwizchip(CW_GET_PHYLINK, &link);
    if(link != PHY_LINK_ON)
    {
      vTaskDelay(pdMS_TO_TICKS(W5500_LINK_POLL_MS));
      retries--;
    }
  }

  if(link != PHY_LINK_ON)
  {
    uint8_t phy = getPHYCFGR();
    (void) USART2_Printf("W5500: no link after 5 s (PHYCFGR=0x%02X)\r\n", (unsigned) phy);
    return W5500_ERR_LINK;
  }

  /* --- Apply static network config --- */
  setSHAR(s_net_info.mac);
  (void) ctlnetwork(CN_SET_NETINFO, (void*) &s_net_info);

  return W5500_OK;
}

const wiz_NetInfo* W5500_GetNetInfo(void)
{
  return &s_net_info;
}

