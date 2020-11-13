#include "blemqttproxy.h"
#include "ble_mqtt.h"

#include "beacon.h"
#include "helperfunctions.h"
#include "lvgl.h"
#include "wifi.h"
#include "stats.h"
#include "axp192.h"

static const char* TAG = "BLE_MQTT";

esp_mqtt_client_handle_t mqtt_client;
EventGroupHandle_t mqtt_evg;

int mqtt_client_publish(esp_mqtt_client_handle_t client, const char *topic, const char *data, int len, int qos, int retain)
{
    EventBits_t uxReturn;
    bool mqtt_connected, wifi_connected;
    int msg_id = -1;

    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

    if(wifi_connected && mqtt_connected){
        msg_id = esp_mqtt_client_publish(mqtt_client, topic, data, len, qos, retain);
        ESP_LOGD(TAG, "mqtt_client_publish, msg_id=%d", msg_id);
        if(msg_id == -1){
            mqtt_packets_fail++;
        } else {
            mqtt_packets_send++;
        }
    }

    return msg_id;
}

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
            bool dont_send_other_values_too = false;

            mqtt_send_adv = true;

            // identifier, maj, min, sensor -> data
            // snprintf(buffer_topic, 128,  "/%s/0x%04x/x%04x/%s", "beac", maj, min, "temp");
            if( (temp < CONFIG_TEMP_LOW) || (temp > CONFIG_TEMP_HIGH) ){
                ESP_LOGE(TAG, "temperature out of range, not send");
                dont_send_other_values_too = true;
            } else {
                snprintf_nowarn(buffer_topic, 128,  CONFIG_MQTT_FORMAT, "beac", maj, min, "temp");
                snprintf_nowarn(buffer_payload, 128, "%.2f", temp);
                msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                }
            }
            if( dont_send_other_values_too || (humidity < CONFIG_HUMIDITY_LOW) || (humidity > CONFIG_HUMIDITY_HIGH) ){
                ESP_LOGE(TAG, "humidity (or temperature) out of range, not send");
            } else {
                snprintf_nowarn(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "humidity");
                snprintf_nowarn(buffer_payload, 128, "%.2f", humidity);
                msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                }
            }
            snprintf_nowarn(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "rssi");
            snprintf_nowarn(buffer_payload, 128, "%d", rssi);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
            if( (battery < CONFIG_BATTERY_LOW) || (battery > CONFIG_BATTERY_HIGH )){
                ESP_LOGE(TAG, "battery out of range, not send");
            } else {
                snprintf_nowarn(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "battery");
                snprintf_nowarn(buffer_payload, 128, "%d", battery);
                msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
                if(msg_id == -1){
                    mqtt_send_adv = false;
                }
            }
        }
    } else {
        ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: WIFI %d MQTT %d, not send", wifi_connected, mqtt_connected );
    }

    return mqtt_send_adv;
}


void send_mqtt_uptime_heap_last_seen(uint8_t num_act_beacon, uint16_t lowest_last_seen_sec, uint16_t lowest_last_send_sec){
    EventBits_t uxReturn;
    int msg_id = 0; UNUSED(msg_id);
    char buffer[32], buffer_topic[32], buffer_payload[32];
    bool wifi_connected, mqtt_connected;
    uint32_t uptime_sec;
    lv_mem_monitor_t mem_mon; UNUSED(mem_mon);

    // send uptime and free heap to MQTT
    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

    if(wifi_connected && mqtt_connected){
        sprintf(buffer, IPSTR_UL, IP2STR(&ipinfo.ip));
        snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "uptime");
        uptime_sec = esp_timer_get_time()/1000000;
        snprintf_nowarn(buffer_payload, 32, "%d", uptime_sec);
        msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

        snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "free_heap");
        snprintf_nowarn(buffer_payload, 32, "%d", esp_get_free_heap_size());
        msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

#ifdef CONFIG_AXP192_PRESENT
        snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "battery");
        snprintf_nowarn(buffer_payload, 32, "%d", (uint16_t) (axp192_get_battery_voltage()*1000));
        msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
#endif

        // LV Memory Monitor
        // lv_mem_monitor(&mem_mon);
        // ESP_LOGD(TAG, "lv_mem_monitor: Memory %d %%, Total %d bytes, used %d bytes, free %d bytes, frag %d %%",
        //     (int) mem_mon.used_pct, (int) mem_mon.total_size,
        //     (int) mem_mon.total_size - mem_mon.free_size, mem_mon.free_size, mem_mon.frag_pct);

        if(num_act_beacon > 0){
            snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "last_seen");
            snprintf_nowarn(buffer_payload, 32, "%d", lowest_last_seen_sec);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

            snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "last_send");
            snprintf_nowarn(buffer_payload, 32, "%d", lowest_last_send_sec);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
        }
    }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    UNUSED(client);

    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
            xEventGroupSetBits(mqtt_evg, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(mqtt_evg, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            // ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
            break;
        case MQTT_EVENT_ANY:
            ESP_LOGI(TAG, "MQTT_EVENT_ANY");
            break;
        default:
            ESP_LOGI(TAG, "MQTT_EVENT default");
            break;
    }
    return ESP_OK;
}

void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .event_handle   = mqtt_event_handler,
        .host           = CONFIG_MQTT_HOST,
        .uri            = CONFIG_MQTT_BROKER_URL,
        .port           = CONFIG_MQTT_PORT,
        .username       = CONFIG_MQTT_USERNAME,
        .password       = CONFIG_MQTT_PASSWORD
    };

    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(mqtt_client);
}
