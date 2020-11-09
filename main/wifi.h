#ifndef __WIFI_H__
#define __WIFI_H__

#define  WIFI_CONNECTED_BIT         (BIT0)

extern uint8_t mac[];
extern EventGroupHandle_t wifi_evg;
extern tcpip_adapter_ip_info_t ipinfo;

void wifi_mqtt_task(void* pvParameters);

#endif // __WIFI_H__