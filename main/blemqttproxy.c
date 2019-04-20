#include <sdkconfig.h>
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
#include "iot_button.h"

#ifdef CONFIG_DISPLAY_SSD1306
#include "iot_i2c_bus.h"
#include "iot_ssd1306.h"
#include "ssd1306_fonts.h"
#endif // CONFIG_DISPLAY_SSD1306

static const char* TAG = "BLEMQTTPROXY";
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define UNUSED(expr) do { (void)(expr); } while (0)

// BLE
const uint8_t uuid_zeros[ESP_UUID_LEN_32] = {0x00, 0x00, 0x00, 0x00};
static uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min);
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

typedef struct  {
    uint8_t     proximity_uuid[4];
    uint16_t    major;
    uint16_t    minor;
    char        name[8];
} ble_beacon_data_t;

typedef struct  {
    int8_t      measured_power;
    float       temp;
    float       humidity;
    uint16_t    battery;
} ble_adv_data_t;

ble_beacon_data_t ble_beacon_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] = {
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_1_MAJ,  CONFIG_BLE_DEVICE_1_MIN,  CONFIG_BLE_DEVICE_1_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_2_MAJ,  CONFIG_BLE_DEVICE_2_MIN,  CONFIG_BLE_DEVICE_2_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_3_MAJ,  CONFIG_BLE_DEVICE_3_MIN,  CONFIG_BLE_DEVICE_3_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_4_MAJ,  CONFIG_BLE_DEVICE_4_MIN,  CONFIG_BLE_DEVICE_4_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_5_MAJ,  CONFIG_BLE_DEVICE_5_MIN,  CONFIG_BLE_DEVICE_5_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_6_MAJ,  CONFIG_BLE_DEVICE_6_MIN,  CONFIG_BLE_DEVICE_6_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_7_MAJ,  CONFIG_BLE_DEVICE_7_MIN,  CONFIG_BLE_DEVICE_7_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_8_MAJ,  CONFIG_BLE_DEVICE_8_MIN,  CONFIG_BLE_DEVICE_8_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_9_MAJ,  CONFIG_BLE_DEVICE_9_MIN,  CONFIG_BLE_DEVICE_9_NAME},
    { {0x01, 0x12, 0x23, 0x34}, CONFIG_BLE_DEVICE_10_MAJ, CONFIG_BLE_DEVICE_10_MIN, CONFIG_BLE_DEVICE_10_NAME},
};
ble_adv_data_t    ble_adv_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED];

// Beacon
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
    int16_t  temp;
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
    .company_id = 0x0059,
    .beacon_type = 0x1502
};

// Display
#define UPDATE_BEAC0     (BIT0)
#define UPDATE_BEAC1     (BIT1)
#define UPDATE_BEAC2     (BIT2)
#define UPDATE_BEAC3     (BIT3)
#define UPDATE_BEAC4     (BIT4)
#define UPDATE_BEAC5     (BIT5)
#define UPDATE_BEAC6     (BIT6)
#define UPDATE_BEAC7     (BIT7)
#define UPDATE_BEAC8     (BIT8)
#define UPDATE_BEAC9     (BIT9)
#define UPDATE_DISPLAY   (BIT10)
EventGroupHandle_t s_values_evg;
static uint8_t s_display_show = 0;  // 0 = off, 1.. beac to show, for array minus 1

// Wifi
static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

// MQTT
static esp_mqtt_client_handle_t s_client;

// Display
#ifdef CONFIG_DISPLAY_SSD1306
static i2c_bus_handle_t i2c_bus = NULL;
static ssd1306_handle_t dev = NULL;
#endif

// Button
#define BUTTON_IO_NUM           0
#define BUTTON_ACTIVE_LEVEL     0


void button_tap_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    s_display_show++;
    s_display_show %= CONFIG_BLE_DEVICE_COUNT_USE+1;

    ESP_LOGI(TAG, "button_tap_cb: %d", s_display_show);
}

#ifdef CONFIG_DISPLAY_SSD1306
esp_err_t ssd1306_update(ssd1306_handle_t dev)
{
    esp_err_t ret;
    char buffer[128];
    ret = iot_ssd1306_clear_screen(dev, 0x00);
    if (ret == ESP_FAIL) {
        return ret;
    }

    if (!s_display_show){
        return iot_ssd1306_refresh_gram(dev);
    }

    int idx = s_display_show - 1;
    snprintf(buffer, 128, "%s:", ble_beacon_data[idx].name);
    iot_ssd1306_draw_string(dev, 0, 0, (const uint8_t*) buffer, 12, 1);
    snprintf(buffer, 128, "%5.1fC, %5.1f%%H", ble_adv_data[idx].temp, ble_adv_data[idx].humidity);
    iot_ssd1306_draw_string(dev, 0, 16, (const uint8_t*) buffer, 12, 1);
    snprintf(buffer, 128, "Batt %4d mV", ble_adv_data[idx].battery);
    iot_ssd1306_draw_string(dev, 0, 32, (const uint8_t*) buffer, 12, 1);
    snprintf(buffer, 128, "RSSI %3d dBm", ble_adv_data[idx].measured_power);
    iot_ssd1306_draw_string(dev, 0, 48, (const uint8_t*) buffer, 12, 1);

    return iot_ssd1306_refresh_gram(dev);
}

static void i2c_bus_init(void)
{
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = OLED_IIC_SDA_NUM;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = OLED_IIC_SCL_NUM;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = OLED_IIC_FREQ_HZ;
    i2c_bus = iot_i2c_bus_create(OLED_IIC_NUM, &conf);
}

static void dev_ssd1306_initialization(void)
{
    ESP_LOGI(TAG, "oled task start!");
    i2c_bus_init();
    dev = iot_ssd1306_create(i2c_bus, 0x3C);
    iot_ssd1306_refresh_gram(dev);
    // ssd1306_show_signs(dev);
    ESP_LOGI(TAG, "oled finish!");
}

static void ssd1306_task(void* pvParameters)
{
    EventBits_t uxBits;
    UNUSED(uxBits);
    dev_ssd1306_initialization();
    while (1) {
        uxBits = xEventGroupWaitBits(s_values_evg,
            UPDATE_BEAC0 | UPDATE_BEAC1 | UPDATE_BEAC2 | UPDATE_BEAC3 |
            UPDATE_BEAC4 | UPDATE_BEAC5 | UPDATE_BEAC6 | UPDATE_BEAC7 |
            UPDATE_BEAC8 | UPDATE_BEAC9 | UPDATE_DISPLAY, pdTRUE, pdFALSE, 0);
        ssd1306_update(dev);
    }
    iot_ssd1306_delete(dev, true);
    vTaskDelete(NULL);
}
#endif // CONFIG_DISPLAY_SSD1306

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
    UNUSED(client);
    int msg_id = 0;

    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_BEFORE_CONNECT:
            ESP_LOGD(TAG, "MQTT_EVENT_BEFORE_CONNECT");
            break;
        case MQTT_EVENT_CONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
            break;
        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGD(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGD(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGD(TAG, "MQTT_EVENT_DATA");
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGD(TAG, "MQTT_EVENT_ERROR");
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
        // .client_id      = "ESP-TEST",
        .username       = CONFIG_MQTT_USERNAME,
        .password       = CONFIG_MQTT_PASSWORD
        // .user_context = (void *)your_context
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(s_client);
}

uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min)
{
    if( (maj == CONFIG_BLE_DEVICE_1_MAJ ) && (min == CONFIG_BLE_DEVICE_1_MIN ) ) return 0;
    if( (maj == CONFIG_BLE_DEVICE_2_MAJ ) && (min == CONFIG_BLE_DEVICE_2_MIN ) ) return 1;
    if( (maj == CONFIG_BLE_DEVICE_3_MAJ ) && (min == CONFIG_BLE_DEVICE_3_MIN ) ) return 2;
    if( (maj == CONFIG_BLE_DEVICE_4_MAJ ) && (min == CONFIG_BLE_DEVICE_4_MIN ) ) return 3;
    if( (maj == CONFIG_BLE_DEVICE_5_MAJ ) && (min == CONFIG_BLE_DEVICE_5_MIN ) ) return 4;
    if( (maj == CONFIG_BLE_DEVICE_6_MAJ ) && (min == CONFIG_BLE_DEVICE_6_MIN ) ) return 5;
    if( (maj == CONFIG_BLE_DEVICE_7_MAJ ) && (min == CONFIG_BLE_DEVICE_7_MIN ) ) return 6;
    if( (maj == CONFIG_BLE_DEVICE_8_MAJ ) && (min == CONFIG_BLE_DEVICE_8_MIN ) ) return 7;
    if( (maj == CONFIG_BLE_DEVICE_9_MAJ ) && (min == CONFIG_BLE_DEVICE_9_MIN ) ) return 8;
    if( (maj == CONFIG_BLE_DEVICE_10_MAJ) && (min == CONFIG_BLE_DEVICE_10_MIN) ) return 9;

    ESP_LOGE(TAG, "beacon_maj_min_to_idx: unknown maj %d min %d", maj, min);

    return 0;
}

void update_adv_data(uint16_t maj, uint16_t min, int8_t measured_power,
    float temp, float humidity, uint16_t battery)
{
    uint8_t  idx = beacon_maj_min_to_idx(maj, min);

    ble_adv_data[idx].measured_power = measured_power;
    ble_adv_data[idx].temp           = temp;
    ble_adv_data[idx].humidity       = humidity;
    ble_adv_data[idx].battery        = battery;

    xEventGroupSetBits(s_values_evg, (EventBits_t) (1 << idx));
}

bool esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head, sizeof(mybeacon_common_head))){
            result = true;
        }
    }

    return result;
}

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x50,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        uint32_t duration = 0;  // scan permanently
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
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
            if(esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                ESP_LOGD(TAG, "mybeacon found");
                esp_ble_mybeacon_t *mybeacon_data = (esp_ble_mybeacon_t*)(scan_result->scan_rst.ble_adv);
                ESP_LOGD(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d",
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major),
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor),
                    scan_result->scan_rst.rssi,
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.temp)/10.,
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.humidity)/10.,
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.x),
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.y),
                    (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.z),
                    ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery) );

                // send via MQTT here:
                uint16_t maj        = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major);
                uint16_t min        = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor);
                float    temp       = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.temp)/10.;
                float    humidity   = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.humidity)/10.;
                uint16_t battery    = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery);

                // identifier, maj, min, sensor -> data
                // snprintf(buffer_topic, 128,  "/%s/0x%04x/x%04x/%s", "beac", maj, min, "temp");
                if( (temp < CONFIG_TEMP_LOW) || (temp > CONFIG_TEMP_HIGH) ){
                    ESP_LOGE(TAG, "temperature out of range, not send");
                } else {
                    snprintf(buffer_topic, 128,  CONFIG_MQTT_FORMAT, "beac", maj, min, "temp");
                    snprintf(buffer_payload, 128, "%+5.1f", temp);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                }

                if( (humidity < CONFIG_HUMIDITY_LOW) || (humidity > CONFIG_HUMIDITY_HIGH) ){
                    ESP_LOGE(TAG, "huidity out of range, not send");
                } else {
                    snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "humidity");
                    snprintf(buffer_payload, 128, "%5.1f", humidity);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                }

                snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "rssi");
                snprintf(buffer_payload, 128, "%d", scan_result->scan_rst.rssi);
                msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);

                snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "battery");
                snprintf(buffer_payload, 128, "%d", battery);
                msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);

                update_adv_data(maj, min, scan_result->scan_rst.rssi, temp, humidity, battery);
            } else {
                ESP_LOGD(TAG, "mybeacon not found");
            }
            break;
        default:
            ESP_LOGE(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT - default");
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
        ESP_LOGE(TAG, "esp_gap_cb - default (%u)", event);
        break;
    }
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    wifi_init();
    mqtt_init();
    s_values_evg = xEventGroupCreate();

    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_PUSH, button_tap_cb, "PUSH");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));

#ifdef CONFIG_DISPLAY_SSD1306
    xTaskCreate(&ssd1306_task, "ssd1306_task", 2048 * 2, NULL, 5, NULL);
#endif // CONFIG_DISPLAY_SSD1306

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&ble_scan_params));
}

