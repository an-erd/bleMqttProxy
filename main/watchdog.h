#ifndef __WATCHDOG_H__
#define __WATCHDOG_H__

#include "esp_timer.h"
#include "esp_event.h"
// #include "blemqttproxy.h"

esp_timer_handle_t periodic_wdt_timer;
void periodic_wdt_timer_callback(void* arg);
void periodic_wdt_timer_start();
#define UPDATE_ESP_RESTART          (BIT0)
#define UPDATE_ESP_MQTT_RESTART     (BIT1)
extern uint8_t esp_restart_mqtt_beacon_to_take;
extern EventGroupHandle_t wdt_evg;

#define WDT_TIMER_DURATION          (CONFIG_WDT_OWN_INTERVAL * 1000000)

void wdt_task(void* pvParameters);

#endif // __WATCHDOG_H__