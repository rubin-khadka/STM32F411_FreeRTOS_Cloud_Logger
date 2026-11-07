/*
 * esp8266.h
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#ifndef ESP8266_ESP8266_H_
#define ESP8266_ESP8266_H_

#include <stdint.h>

typedef enum
{
  ESP8266_OK = 0,
  ESP8266_TIMEOUT,
  ESP8266_ERROR,
  ESP8266_NO_RESPONSE,
  ESP8266_NOT_INITIALIZED
} ESP8266_Status_t;

void ESP8266_Init(void);
ESP8266_Status_t ESP8266_Test(void);
ESP8266_Status_t ESP8266_ConnectWiFi(const char *ssid, const char *password, uint32_t timeout_ms);
ESP8266_Status_t ESP8266_GetIP(char *ip_buf, uint16_t buf_len);
ESP8266_Status_t ESP8266_TCPConnect(const char *host, uint16_t port, uint32_t timeout_ms);
ESP8266_Status_t ESP8266_SendCommand(const char *cmd, const char *ack, uint32_t timeout_ms);
ESP8266_Status_t ESP8266_SendBinary(const uint8_t *data, uint16_t len, const char *ack, uint32_t timeout_ms);

#endif /* ESP8266_ESP8266_H_ */
