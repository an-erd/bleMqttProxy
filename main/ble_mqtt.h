#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "mqtt_client.h"


// MQTT
extern esp_mqtt_client_handle_t mqtt_client;
extern EventGroupHandle_t mqtt_evg;
#define  MQTT_CONNECTED_BIT      (BIT0)


extern uint32_t mqtt_packets_send;
extern uint32_t mqtt_packets_fail;

#if CONFIG_USE_MQTT==1
int mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);
bool send_to_mqtt(uint8_t idx, uint16_t maj, uint16_t min, float temp, float humidity, uint16_t battery, int8_t rssi);
#endif

#endif // __MQTT_H__
