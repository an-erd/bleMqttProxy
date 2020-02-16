#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_assert.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "ble_mqtt.h"
#include "beacon.h"

static const char *TAG = "ble_mqtt";

uint32_t mqtt_packets_send = 0;
uint32_t mqtt_packets_fail = 0;

esp_mqtt_client_handle_t mqtt_client;
EventGroupHandle_t mqtt_evg;
extern EventGroupHandle_t wifi_evg;
#define  WIFI_CONNECTED_BIT         (BIT0)

#if CONFIG_USE_MQTT==1

bool send_to_mqtt(uint8_t idx, uint16_t maj, uint16_t min, float temp, float humidity, uint16_t battery, int8_t rssi)
{
    bool mqtt_send_adv = false;

    EventBits_t uxReturn;
    bool mqtt_connected, wifi_connected;

    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

    if(wifi_connected && mqtt_connected){

        uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_beacons[idx].adv_data.mqtt_last_send)/1000000;
        if( (mqtt_last_send_sec_gone > CONFIG_MQTT_MIN_TIME_INTERVAL_BETWEEN_MESSAGES)
            || (ble_beacons[idx].adv_data.mqtt_last_send == 0)){
            int msg_id = 0;
            char buffer_topic[128];
            char buffer_payload[128];

            mqtt_send_adv = true;

            // identifier, maj, min, sensor -> data
            // snprintf(buffer_topic, 128,  "/%s/0x%04x/x%04x/%s", "beac", maj, min, "temp");
            if( (temp < CONFIG_TEMP_LOW) || (temp > CONFIG_TEMP_HIGH) ){
                ESP_LOGE(TAG, "temperature out of range, not send");
            } else {
                snprintf(buffer_topic, 128,  CONFIG_MQTT_FORMAT, "beac", maj, min, "temp");
                snprintf(buffer_payload, 128, "%.2f", temp);
                msg_id = esp_mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                    mqtt_packets_fail++;
                } else {
                    mqtt_packets_send++;
                }
            }
            if( (humidity < CONFIG_HUMIDITY_LOW) || (humidity > CONFIG_HUMIDITY_HIGH) ){
                ESP_LOGE(TAG, "humidity out of range, not send");
            } else {
                snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "humidity");
                snprintf(buffer_payload, 128, "%.2f", humidity);
                msg_id = esp_mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                    mqtt_packets_fail++;
                } else {
                    mqtt_packets_send++;
                }
            }
            snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "rssi");
            snprintf(buffer_payload, 128, "%d", rssi);
            msg_id = esp_mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
            ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
            if( (battery < CONFIG_BATTERY_LOW) || (battery > CONFIG_BATTERY_HIGH )){
                ESP_LOGE(TAG, "battery out of range, not send");
            } else {
                snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "battery");
                snprintf(buffer_payload, 128, "%d", battery);
                msg_id = esp_mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                    mqtt_packets_fail++;
                } else {
                    mqtt_packets_send++;
                }
            }
        }
    } else {
        ESP_LOGI(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: WIFI %d MQTT %d, not send", wifi_connected, mqtt_connected );
    }

    return mqtt_send_adv;
}

#endif // CONFIG_USE_MQTT
