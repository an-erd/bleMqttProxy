#ifndef __MQTT_H__
#define __MQTT_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "mqtt_client.h"


// MQTT
extern esp_mqtt_client_handle_t s_client;
extern EventGroupHandle_t s_mqtt_evg;
#define  MQTT_CONNECTED_BIT      (BIT0)


extern uint16_t mqtt_packets_send;
extern uint16_t mqtt_packets_fail;

#endif // __MQTT_H__
