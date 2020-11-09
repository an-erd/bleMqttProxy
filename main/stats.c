#include "blemqttproxy.h"

#include "stats.h"
#include "ble_mqtt.h"

/* Wifi */
uint16_t wifi_connections_count_connect = 0;
uint16_t wifi_connections_count_disconnect = 0;

/* WiFi AP */
uint16_t wifi_ap_connections = 0;

/* MQTT */
uint32_t mqtt_packets_send = 0;
uint32_t mqtt_packets_fail = 0;

void clear_stats_values()
{
    wifi_connections_count_connect = 0;
    wifi_connections_count_disconnect = 0;
    wifi_ap_connections = 0;
    mqtt_packets_send = 0;
    mqtt_packets_fail = 0;
}
