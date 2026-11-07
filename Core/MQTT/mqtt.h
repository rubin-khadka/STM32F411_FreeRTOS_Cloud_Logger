/*
 * mqtt.h
 *
 *  Created on: Nov 7, 2026
 *      Author: Rubin Khadka
 */

#ifndef MQTT_MQTT_H_
#define MQTT_MQTT_H_

#include <stdint.h>

uint16_t MQTT_BuildConnect(
    uint8_t *buf,
    uint16_t buf_len,
    const char *client_id,
    const char *username,
    const char *password,
    uint16_t keepalive_s);

uint16_t MQTT_BuildPublish(uint8_t *buf, uint16_t buf_len, const char *topic, const char *payload);

uint16_t MQTT_BuildPingReq(uint8_t *buf, uint16_t buf_len);
uint16_t MQTT_BuildDisconnect(uint8_t *buf, uint16_t buf_len);

#endif /* MQTT_MQTT_H_ */
