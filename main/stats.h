#ifndef __STATS_H__
#define __STATS_H__

extern uint16_t wifi_connections_count_connect;     // count number of WiFi connects
extern uint16_t wifi_connections_count_disconnect;  // count number of WiFi disconnects
extern uint16_t wifi_ap_connections;                // keep trackm of the current AP connections
extern uint32_t mqtt_packets_send;                  // successful MQTT packets send
extern uint32_t mqtt_packets_fail;                  // failed MQTT packetw

void clear_stats_values();

#endif // __STATS_H__