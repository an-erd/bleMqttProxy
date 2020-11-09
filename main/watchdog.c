#include "blemqttproxy.h"
#include "watchdog.h"
#include "beacon.h"
#include "ble_mqtt.h"
#include "wifi.h"
#include "helperfunctions.h"

static const char *TAG = "WATCHDOG";

uint8_t esp_restart_mqtt_beacon_to_take = UNKNOWN_BEACON;
EventGroupHandle_t wdt_evg;

void periodic_wdt_timer_start(){
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_wdt_timer, WDT_TIMER_DURATION));
}

void periodic_wdt_timer_callback(void* arg)
{
    EventBits_t uxSet, uxReturn;
    bool wifi_connected, mqtt_connected;
    uint8_t beacon_to_take_seen = UNKNOWN_BEACON;
    uint8_t beacon_to_take_send = UNKNOWN_BEACON;
    uint16_t lowest_last_seen_sec = CONFIG_WDT_LAST_SEEN_THRESHOLD;
    uint16_t lowest_last_send_sec = CONFIG_WDT_LAST_SEND_THRESHOLD;
    uint16_t temp_last_seen_sec, temp_last_send_sec;
    uint8_t num_active = num_active_beacon();

    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            temp_last_seen_sec = (esp_timer_get_time() - ble_beacons[i].adv_data.last_seen)/1000000;
            if(temp_last_seen_sec < lowest_last_seen_sec){
                beacon_to_take_seen = i;
                lowest_last_seen_sec = temp_last_seen_sec;
            }
            temp_last_send_sec = (esp_timer_get_time() - ble_beacons[i].adv_data.mqtt_last_send)/1000000;
            if(temp_last_send_sec < lowest_last_send_sec){
                beacon_to_take_send = i;
                lowest_last_send_sec = temp_last_send_sec;
            }
        }
    }

#ifdef CONFIG_WDT_SEND_REGULAR_UPTIME_HEAP_MQTT
    send_mqtt_uptime_heap_last_seen(num_active, lowest_last_seen_sec, lowest_last_send_sec);
#endif

    if(num_active == 0){
        ESP_LOGD(TAG, "periodic_wdt_timer_callback: no beacon active <");
        return;
    }

    ESP_LOGD(TAG, "check 1: lowest_last_seen_sec %d, lowest_last_send_sec %d, beacon_to_take_seen %d, beacon_to_take_send %d",
        lowest_last_seen_sec, lowest_last_send_sec, beacon_to_take_seen, beacon_to_take_send );

    if((lowest_last_seen_sec >= CONFIG_WDT_LAST_SEEN_THRESHOLD) || (lowest_last_send_sec >= CONFIG_WDT_LAST_SEND_THRESHOLD)){
        ESP_LOGD(TAG, "check 2: lowest_last_seen_sec >= CONFIG_WDT_LAST_SEEN_THRESHOLD: %d, lowest_last_send_sec >= CONFIG_WDT_LAST_SEND_THRESHOLD: %d",
            (lowest_last_seen_sec >= CONFIG_WDT_LAST_SEEN_THRESHOLD), (lowest_last_send_sec >= CONFIG_WDT_LAST_SEND_THRESHOLD) );

        uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
        mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

        uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
        wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

        ESP_LOGD(TAG, "periodic_wdt_timer_callback: last seen > threshold: %d sec, last send > threshold: %d sec, WIFI: %s, MQTT (enabled/connected): %s/%s",
            lowest_last_seen_sec, lowest_last_send_sec,
            (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));

        uxSet = 0;
        if( ((CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD == 1) && (lowest_last_seen_sec >= CONFIG_WDT_LAST_SEEN_THRESHOLD))
            || ((CONFIG_WDT_REBOOT_LAST_SEND_THRESHOLD == 1) && (lowest_last_send_sec >= CONFIG_WDT_LAST_SEND_THRESHOLD)) ){
            uxSet |= UPDATE_ESP_RESTART;
            ESP_LOGD(TAG, "periodic_wdt_timer_callback: reboot/mqtt flag set: %d, check 1", uxSet);
        }

        if(CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT){
            uxSet |= UPDATE_ESP_MQTT_RESTART;
            ESP_LOGD(TAG, "periodic_wdt_timer_callback: reboot/mqtt flag set: %d, check 2", uxSet);
        }

        if(uxSet){
            ESP_LOGD(TAG, "periodic_wdt_timer_callback: reboot/mqtt flag set: %d, check 3", uxSet);
            esp_restart_mqtt_beacon_to_take = MIN(beacon_to_take_seen, beacon_to_take_send);
            xEventGroupSetBits(wdt_evg, uxSet);
        }
    }
}

#if ((CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1) || (CONFIG_WDT_REBOOT_LAST_SEND_THRESHOLD==1) || (CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1) )
void wdt_task(void* pvParameters)
{
    EventBits_t uxBits;
    char buffer[32], buffer_topic[32], buffer_payload[32];
    uint32_t uptime_sec;
    int msg_id = 0; UNUSED(msg_id);
    periodic_wdt_timer_start();

    while (1) {
        uxBits = xEventGroupWaitBits(wdt_evg, UPDATE_ESP_RESTART | UPDATE_ESP_MQTT_RESTART, pdTRUE, pdFALSE, portMAX_DELAY);

        ESP_LOGD(TAG, "ota_task uxTaskGetStackHighWaterMark '%d'", uxTaskGetStackHighWaterMark(NULL));

        if(uxBits & UPDATE_ESP_MQTT_RESTART){
            ESP_LOGI(TAG, "wdt_task: reboot send mqtt flag is set -> send MQTT message");
            uptime_sec = esp_timer_get_time()/1000000;
            sprintf(buffer, IPSTR_UL, IP2STR(&ipinfo.ip));
            snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "reboot");
            snprintf_nowarn(buffer_payload, 128, "%d", uptime_sec);
            ESP_LOGD(TAG, "wdt_task: MQTT message to be send reg. REBOOT: %s %s", buffer_topic, buffer_payload);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
            fflush(stdout);
        }

        if(uxBits & UPDATE_ESP_RESTART){
            ESP_LOGI(TAG, "wdt_task: reboot flag is set -> esp_restart() -> REBOOT");
            fflush(stdout);
            esp_restart();
        }
    }
    vTaskDelete(NULL);
}
#endif
