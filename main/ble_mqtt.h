#ifndef __MQTT_H__
#define __MQTT_H__

#include "blemqttproxy.h"

extern esp_mqtt_client_handle_t mqtt_client;
extern EventGroupHandle_t mqtt_evg;
#define  MQTT_CONNECTED_BIT      (BIT0)

int mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain);
bool send_to_mqtt(uint8_t idx, uint16_t maj, uint16_t min, float temp, float humidity, uint16_t battery, int8_t rssi);
void send_mqtt_uptime_heap_last_seen(uint8_t num_act_beacon, uint16_t lowest_last_seen_sec, uint16_t lowest_last_send_sec);
void mqtt_init(void);

#endif // __MQTT_H__
