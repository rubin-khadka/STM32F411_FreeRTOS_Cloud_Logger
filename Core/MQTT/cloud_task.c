/*
 * cloud_task.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "cloud_task.h"
#include "esp8266.h"
#include "mqtt.h"
#include "sensor_data.h"
#include "usart2.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "secrets.h"

/* --- Secrets --- */
#define WIFI_SSID           SECRET_WIFI_SSID
#define WIFI_PASS           SECRET_WIFI_PASS
#define MQTT_CLIENT_ID      SECRET_MQTT_CLIENT_ID
#define MQTT_USERNAME       SECRET_MQTT_USERNAME
#define MQTT_PASSWORD       SECRET_MQTT_PASSWORD

#define MQTT_BROKER         "mqtt3.thingspeak.com"
#define MQTT_PORT           1883U
#define MQTT_KEEPALIVE_S    60U

#define TOPIC_DHT           "channels/3569863/publish"
#define TOPIC_IMU           "channels/3569865/publish"

#define CLOUD_CYCLE_MS      15000U

/* ---------- Formatting helpers ---------- */

static char* put_str(char *p, const char *s)
{
  while(*s != '\0')
  {
    *p++ = *s++;
  }
  return p;
}

static char* put_scaled(char *p, int32_t scaled, uint8_t decimals)
{
  if(scaled < 0)
  {
    *p++ = '-';
    scaled = -scaled;
  }

  uint32_t div = 1U;
  for(uint8_t i = 0U; i < decimals; i++)
  {
    div *= 10U;
  }

  uint32_t ip = (uint32_t) scaled / div;
  uint32_t fp = (uint32_t) scaled % div;

  if(ip == 0U)
  {
    *p++ = '0';
  }
  else
  {
    char tmp[10];
    uint8_t n = 0U;
    while(ip > 0U)
    {
      tmp[n++] = (char) ('0' + (ip % 10U));
      ip /= 10U;
    }
    while(n > 0U)
    {
      *p++ = tmp[--n];
    }
  }

  if(decimals > 0U)
  {
    *p++ = '.';
    div /= 10U;
    while(div > 0U)
    {
      *p++ = (char) ('0' + (fp / div));
      fp %= div;
      div /= 10U;
    }
  }
  return p;
}

static int32_t round_scaled(float v, float scale)
{
  float s = v * scale;
  s += (s >= 0.0f) ? 0.5f : -0.5f;
  return (int32_t) s;
}

/* ---------- Payloads ---------- */

static void build_dht_payload(const SensorData_t *s, char *buf)
{
  char *p = buf;
  p = put_str(p, "field1=");
  p = put_scaled(p, round_scaled(s->temp_c, 10.0f), 1U);
  p = put_str(p, "&field2=");
  p = put_scaled(p, round_scaled(s->hum_pct, 10.0f), 1U);
  *p = '\0';
}

static void build_imu_payload(const SensorData_t *s, char *buf)
{
  char *p = buf;
  p = put_str(p, "field1=");
  p = put_scaled(p, round_scaled(s->ax_g, 1000.0f), 3U);
  p = put_str(p, "&field2=");
  p = put_scaled(p, round_scaled(s->ay_g, 1000.0f), 3U);
  p = put_str(p, "&field3=");
  p = put_scaled(p, round_scaled(s->az_g, 1000.0f), 3U);
  p = put_str(p, "&field4=");
  p = put_scaled(p, round_scaled(s->gx_dps, 100.0f), 2U);
  p = put_str(p, "&field5=");
  p = put_scaled(p, round_scaled(s->gy_dps, 100.0f), 2U);
  p = put_str(p, "&field6=");
  p = put_scaled(p, round_scaled(s->gz_dps, 100.0f), 2U);
  *p = '\0';
}

/* ---------- Connect ---------- */

static bool cloud_connect(void)
{
  if(ESP8266_ConnectWiFi(WIFI_SSID, WIFI_PASS, 20000U) != ESP8266_OK)
  {
    (void) USART2_Printf("CLOUD: WiFi FAILED\r\n");
    return false;
  }

  if(ESP8266_TCPConnect(MQTT_BROKER, MQTT_PORT, 8000U) != ESP8266_OK)
  {
    (void) USART2_Printf("CLOUD: TCP FAILED\r\n");
    return false;
  }

  uint8_t pkt[160];
  uint16_t len = MQTT_BuildConnect(pkt, sizeof(pkt),
  MQTT_CLIENT_ID, MQTT_USERNAME,
  MQTT_PASSWORD, MQTT_KEEPALIVE_S);
  if(len == 0U)
  {
    (void) USART2_Printf("CLOUD: MQTT build FAILED\r\n");
    return false;
  }

  char cmd[32];
  (void) snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned) len);
  if(ESP8266_SendCommand(cmd, ">", 2000U) != ESP8266_OK)
  {
    (void) USART2_Printf("CLOUD: CIPSEND prompt FAILED\r\n");
    return false;
  }

  if(ESP8266_SendBinary(pkt, len, "\x20\x02", 10000U) != ESP8266_OK)
  {
    (void) USART2_Printf("CLOUD: CONNACK FAILED\r\n");
    return false;
  }

  return true;
}

/* ---------- Publish ---------- */

static bool cloud_publish(const char *topic, const char *payload)
{
  uint8_t pkt[256];
  uint16_t len = MQTT_BuildPublish(pkt, sizeof(pkt), topic, payload);
  if(len == 0U)
  {
    return false;
  }

  char cmd[32];
  (void) snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", (unsigned) len);
  if(ESP8266_SendCommand(cmd, ">", 2000U) != ESP8266_OK)
  {
    return false;
  }
  if(ESP8266_SendBinary(pkt, len, "SEND OK", 5000U) != ESP8266_OK)
  {
    return false;
  }

  return true;
}

/* ---------- Task ---------- */

void vCloudTask(void *pv)
{
  (void) pv;

  vTaskDelay(pdMS_TO_TICKS(2000));
  ESP8266_Init();

  if(!cloud_connect())
  {
    (void) USART2_Printf("CLOUD: initial connect FAILED — parking\r\n");
    for(;;)
    {
      vTaskDelay(pdMS_TO_TICKS(10000));
    }
  }

  TickType_t last_wake = xTaskGetTickCount();
  SensorData_t snap;
  char payload[160];
  uint32_t n = 0U;

  for(;;)
  {
    SensorData_GetSnapshot(&snap);
    bool any_fail = false;

    if(snap.dht_valid)
    {
      build_dht_payload(&snap, payload);
      if(!cloud_publish(TOPIC_DHT, payload))
      {
        (void) USART2_Printf("CLOUD: [%lu] DHT FAILED\r\n", (unsigned long) n);
        any_fail = true;
      }
    }

    if(snap.imu_valid)
    {
      build_imu_payload(&snap, payload);
      if(!cloud_publish(TOPIC_IMU, payload))
      {
        (void) USART2_Printf("CLOUD: [%lu] IMU FAILED\r\n", (unsigned long) n);
        any_fail = true;
      }
    }

    if(any_fail)
    {
      (void) USART2_Printf("CLOUD: reconnecting...\r\n");
      if(!cloud_connect())
      {
        (void) USART2_Printf("CLOUD: reconnect FAILED — parking\r\n");
        for(;;)
        {
          vTaskDelay(pdMS_TO_TICKS(10000));
        }
      }
    }

    n++;
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(CLOUD_CYCLE_MS));
  }
}

