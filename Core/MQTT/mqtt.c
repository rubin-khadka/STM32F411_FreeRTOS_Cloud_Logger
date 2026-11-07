/*
 * mqtt.c
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#include "mqtt.h"
#include <string.h>

static uint8_t mqtt_encode_len(uint8_t *buf, uint32_t len)
{
  uint8_t n = 0;
  do
  {
    uint8_t d = (uint8_t) (len % 128U);
    len /= 128U;
    if(len > 0U)
    {
      d |= 0x80U;
    }
    buf[n++] = d;
  }
  while((len > 0U) && (n < 4U));
  return n;
}

static void mqtt_write_u16(uint8_t *buf, uint16_t v)
{
  buf[0] = (uint8_t) (v >> 8U);
  buf[1] = (uint8_t) (v & 0xFFU);
}

uint16_t MQTT_BuildConnect(
    uint8_t *buf,
    uint16_t buf_len,
    const char *client_id,
    const char *username,
    const char *password,
    uint16_t keepalive_s)
{
  if((buf == NULL) || (client_id == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  uint16_t cid_len = (uint16_t) strlen(client_id);
  uint16_t un_len = (username != NULL) ? (uint16_t) strlen(username) : 0U;
  uint16_t pw_len = (password != NULL) ? (uint16_t) strlen(password) : 0U;

  uint32_t rem = 10U + 2U + cid_len;
  if(un_len > 0U)
  {
    rem += 2U + un_len;
  }
  if(pw_len > 0U)
  {
    rem += 2U + pw_len;
  }

  if((1U + 4U + rem) > buf_len)
  {
    return 0U;
  }

  uint16_t i = 0U;
  buf[i++] = 0x10U;
  i = (uint16_t) (i + mqtt_encode_len(&buf[i], rem));

  mqtt_write_u16(&buf[i], 4U);
  i = (uint16_t) (i + 2U);
  buf[i++] = 'M';
  buf[i++] = 'Q';
  buf[i++] = 'T';
  buf[i++] = 'T';

  buf[i++] = 0x04U;

  uint8_t flags = 0x02U;
  if(un_len > 0U)
  {
    flags |= 0x80U;
  }
  if(pw_len > 0U)
  {
    flags |= 0x40U;
  }
  buf[i++] = flags;

  mqtt_write_u16(&buf[i], keepalive_s);
  i = (uint16_t) (i + 2U);

  mqtt_write_u16(&buf[i], cid_len);
  i = (uint16_t) (i + 2U);
  (void) memcpy(&buf[i], client_id, cid_len);
  i = (uint16_t) (i + cid_len);

  if(un_len > 0U)
  {
    mqtt_write_u16(&buf[i], un_len);
    i = (uint16_t) (i + 2U);
    (void) memcpy(&buf[i], username, un_len);
    i = (uint16_t) (i + un_len);
  }

  if(pw_len > 0U)
  {
    mqtt_write_u16(&buf[i], pw_len);
    i = (uint16_t) (i + 2U);
    (void) memcpy(&buf[i], password, pw_len);
    i = (uint16_t) (i + pw_len);
  }

  return i;
}

uint16_t MQTT_BuildPublish(uint8_t *buf, uint16_t buf_len, const char *topic, const char *payload)
{
  if((buf == NULL) || (topic == NULL) || (payload == NULL) || (buf_len == 0U))
  {
    return 0U;
  }

  uint16_t tlen = (uint16_t) strlen(topic);
  uint16_t plen = (uint16_t) strlen(payload);

  uint32_t rem = 2U + tlen + plen;

  if((1U + 4U + rem) > buf_len)
  {
    return 0U;
  }

  uint16_t i = 0U;
  buf[i++] = 0x30U;
  i = (uint16_t) (i + mqtt_encode_len(&buf[i], rem));

  mqtt_write_u16(&buf[i], tlen);
  i = (uint16_t) (i + 2U);
  (void) memcpy(&buf[i], topic, tlen);
  i = (uint16_t) (i + tlen);

  (void) memcpy(&buf[i], payload, plen);
  i = (uint16_t) (i + plen);

  return i;
}

uint16_t MQTT_BuildPingReq(uint8_t *buf, uint16_t buf_len)
{
  if((buf == NULL) || (buf_len < 2U))
  {
    return 0U;
  }
  buf[0] = 0xC0U;
  buf[1] = 0x00U;
  return 2U;
}

uint16_t MQTT_BuildDisconnect(uint8_t *buf, uint16_t buf_len)
{
  if((buf == NULL) || (buf_len < 2U))
  {
    return 0U;
  }
  buf[0] = 0xE0U;
  buf[1] = 0x00U;
  return 2U;
}
