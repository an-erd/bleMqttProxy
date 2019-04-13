#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_log.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"


static const char* TAG = "BLEMQTTPROXY";
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
const uint8_t uuid_zeros[ESP_UUID_LEN_32] = {0x00, 0x00, 0x00, 0x00};

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;
static esp_mqtt_client_handle_t s_client;


static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);

            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
}



static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;

    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGI(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
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
        .client_id      = "ESP-TEST",
        .username       = CONFIG_MQTT_USERNAME,
        .password       = CONFIG_MQTT_PASSWORD
        // .user_context = (void *)your_context
    };

    // esp_mqtt_client_handle_t
    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(s_client);
}

// Beacon data
typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
}__attribute__((packed)) esp_ble_mybeacon_head_t;

typedef struct {
    uint8_t proximity_uuid[4];
    uint16_t major;
    uint16_t minor;
    int8_t measured_power;
}__attribute__((packed)) esp_ble_mybeacon_vendor_t;

typedef struct {
    uint16_t temp;
    uint16_t humidity;
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint16_t battery;
}__attribute__((packed)) esp_ble_mybeacon_payload_t;

typedef struct {
    esp_ble_mybeacon_head_t     mybeacon_head;
    esp_ble_mybeacon_vendor_t   mybeacon_vendor;
    esp_ble_mybeacon_payload_t  mybeacon_payload;
}__attribute__((packed)) esp_ble_mybeacon_t;


esp_ble_mybeacon_head_t mybeacon_common_head = {
    .flags = {0x02, 0x01, 0x04},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x0059,   // ENDIAN_CHANGE_U16 ?
    .beacon_type = 0x1502   // ENDIAN_CHANGE_U16 ?
};

esp_ble_mybeacon_vendor_t vendor_config = {
    .proximity_uuid = {0x01, 0x12, 0x23, 0x34},
    .major = 0x0102, //ENDIAN_CHANGE_U16(ESP_MAJOR), //Major=ESP_MAJOR
    .minor = 0x0304, //ENDIAN_CHANGE_U16(ESP_MINOR), //Minor=ESP_MINOR
    .measured_power = 0xC5
};

bool esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head, sizeof(mybeacon_common_head))){
            result = true;
        }
    }

    return result;
}

esp_err_t esp_ble_config_mybeacon_data (esp_ble_mybeacon_vendor_t *vendor_config, esp_ble_mybeacon_t *ibeacon_adv_data){
    if ((vendor_config == NULL) || (ibeacon_adv_data == NULL) || (!memcmp(vendor_config->proximity_uuid, uuid_zeros, sizeof(uuid_zeros)))){
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(&ibeacon_adv_data->mybeacon_head, &mybeacon_common_head, sizeof(esp_ble_mybeacon_head_t));
    memcpy(&ibeacon_adv_data->mybeacon_vendor, vendor_config, sizeof(esp_ble_mybeacon_vendor_t));

    return ESP_OK;
}




///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,   // BLE_SCAN_TYPE_ACTIVE, BLE_SCAN_TYPE_PASSIVE
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

uint8_t *adv_data = NULL;
uint8_t adv_data_len = 0;

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        //the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Adv start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        int msg_id = 0;
        char buffer_topic[128];
        char buffer_payload[128];

        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");
            // adv_data = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_data_len);
            // ESP_LOGI(TAG, "searched Device Name Len %d", adv_data_len);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, adv_data, adv_data_len, ESP_LOG_INFO);
            // ESP_LOG_BUFFER_HEXDUMP(TAG, scan_result->scan_rst.ble_adv, 31, ESP_LOG_INFO);

            if(esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                ESP_LOGD(TAG, "mybeacon found");
//                ESP_LOG_BUFFER_HEXDUMP(TAG, scan_result->scan_rst.ble_adv, 31, ESP_LOG_INFO);
                esp_ble_mybeacon_t *mybeacon_data = (esp_ble_mybeacon_t*)(scan_result->scan_rst.ble_adv);
                ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %3d | x %+6d | y %+6d | z %+6d | batt %4d",
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major),
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor),
                    scan_result->scan_rst.rssi,
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.temp)/10.,
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.humidity),
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.x),
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.y),
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.z),
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery) );

                    // send via MQTT here:
                    uint16_t maj = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major);
                    uint16_t min = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor);

                    // "/%s/0x%04/x%04x/%s"
                    // identifier, maj, min, sensor -> data
                    snprintf(buffer_topic, 128,  "/%s/0x%04x/x%04x/%s", "beac", maj, min, "temp");
                    snprintf(buffer_payload, 128, "%+5.1f", ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.temp)/10.);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

                    snprintf(buffer_topic, 128, "/%s/0x%04x/x%04x/%s", "beac", maj, min, "humidity");
                    snprintf(buffer_payload, 128, "%d", ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.humidity));
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

                    snprintf(buffer_topic, 128, "/%s/0x%04x/x%04x/%s", "beac", maj, min, "rssi");
                    snprintf(buffer_payload, 128, "%d", scan_result->scan_rst.rssi);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

                    snprintf(buffer_topic, 128, "/%s/0x%04x/x%04x/%s", "beac", maj, min, "battery");
                    snprintf(buffer_payload, 128, "%d", ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery));
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            } else {
//                ESP_LOGI(TAG, "mybeacon not found");
            }
            break;
        default:
            ESP_LOGI(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT - default");
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;

    default:
        ESP_LOGI(TAG, "esp_gap_cb - default (%u)", event);
        break;
    }
}


void ble_beacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(TAG, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(TAG, "gap register error: %s", esp_err_to_name(status));
        return;
    }

}

void ble_beacon_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_beacon_appRegister();
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    wifi_init();
    mqtt_init();

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_bt_controller_init(&bt_cfg);
    esp_bt_controller_enable(ESP_BT_MODE_BLE);

    ble_beacon_init();

    esp_ble_gap_set_scan_params(&ble_scan_params);
}

