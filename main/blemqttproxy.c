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
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "mqtt_client.h"
#include "iot_button.h"
#include <inttypes.h>

#ifdef CONFIG_DISPLAY_SSD1306
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#endif // CONFIG_DISPLAY_SSD1306

static const char* TAG = "BLEMQTTPROXY";
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define MSB_16(a) (((a) & 0xFF00) >> 8)
#define LSB_16(a) ((a) & 0x00FF)
#define UNUSED(expr) do { (void)(expr); } while (0)

#define SHT3_GET_TEMPERATURE_VALUE(temp_msb, temp_lsb) \
    (-45+(((int16_t)temp_msb << 8) | ((int16_t)temp_lsb ))*175/(float)0xFFFF)

#define SHT3_GET_HUMIDITY_VALUE(humidity_msb, humidity_lsb) \
    ((((int16_t)humidity_msb << 8) | ((int16_t)humidity_lsb))*100/(float)0xFFFF)

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
    int64_t     last_seen;
} ble_adv_data_t;

ble_beacon_data_t ble_beacon_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] = {
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_1_MAJ,  CONFIG_BLE_DEVICE_1_MIN,  CONFIG_BLE_DEVICE_1_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_2_MAJ,  CONFIG_BLE_DEVICE_2_MIN,  CONFIG_BLE_DEVICE_2_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_3_MAJ,  CONFIG_BLE_DEVICE_3_MIN,  CONFIG_BLE_DEVICE_3_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_4_MAJ,  CONFIG_BLE_DEVICE_4_MIN,  CONFIG_BLE_DEVICE_4_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_5_MAJ,  CONFIG_BLE_DEVICE_5_MIN,  CONFIG_BLE_DEVICE_5_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_6_MAJ,  CONFIG_BLE_DEVICE_6_MIN,  CONFIG_BLE_DEVICE_6_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_7_MAJ,  CONFIG_BLE_DEVICE_7_MIN,  CONFIG_BLE_DEVICE_7_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_8_MAJ,  CONFIG_BLE_DEVICE_8_MIN,  CONFIG_BLE_DEVICE_8_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_9_MAJ,  CONFIG_BLE_DEVICE_9_MIN,  CONFIG_BLE_DEVICE_9_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_10_MAJ, CONFIG_BLE_DEVICE_10_MIN, CONFIG_BLE_DEVICE_10_NAME},
};
ble_adv_data_t    ble_adv_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] = { 0 };

// Beacon
typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
}__attribute__((packed)) esp_ble_mybeacon_head_t;

typedef struct {
    uint8_t  proximity_uuid[4];
    uint16_t major;
    uint16_t minor;
    int8_t   measured_power;
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
    .company_id = 0x0059,
    .beacon_type = 0x1502
};

esp_ble_mybeacon_vendor_t mybeacon_common_vendor = {
    .proximity_uuid ={CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4},
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

// 0 = off, 1.. beac to show, for array minus 1, num+1 = app version, num+2 = last seen, num+3 = display test
static uint8_t s_display_show = 0;

// Wifi
static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

// MQTT
static esp_mqtt_client_handle_t s_client;

// Timer
static esp_timer_handle_t periodic_timer;
static bool periodic_timer_running = false;
static volatile bool run_periodic_timer = false;
static void periodic_timer_callback(void* arg);
static void periodic_timer_start();
static void periodic_timer_stop();
#define UPDATE_LAST_SEEN_INTERVAL   250000

// Button
#define BUTTON_IO_NUM           0
#define BUTTON_ACTIVE_LEVEL     0
static int64_t time_button_long_press = 0;  // long button press -> empty display

void button_push_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    time_button_long_press = esp_timer_get_time() + CONFIG_LONG_PRESS_TIME * 1000;

    ESP_LOGD(TAG, "button_push_cb");
}

void button_release_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    if(esp_timer_get_time() < time_button_long_press){
        s_display_show++;
#ifndef CONFIG_DISPLAY_TIME_TEST
        s_display_show %= CONFIG_BLE_DEVICE_COUNT_USE+3;
#else
        s_display_show %= CONFIG_BLE_DEVICE_COUNT_USE+4; // add extra test screen
#endif
    } else {
        s_display_show = 0;
    }

    if(s_display_show == 0){
        // empty screen
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
        run_periodic_timer = false;
    } else if(s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+1){
        // app data, version, mac addr
        run_periodic_timer = false;
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
    } else if(s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+2){
        // last seen, update regularly
        run_periodic_timer = true;
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
#ifdef CONFIG_DISPLAY_TIME_TEST
    } else if(s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+3){
        // last seen, update regularly
        run_periodic_timer = true;
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
#endif
    } else {
        // beacon screen
        run_periodic_timer = false;
        xEventGroupSetBits(s_values_evg, (EventBits_t) (1 << (s_display_show-1) ));
    }

    ESP_LOGD(TAG, "button_release_cb: s_display_show %d", s_display_show);
}

void periodic_timer_start()
{
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, UPDATE_LAST_SEEN_INTERVAL));
}

void periodic_timer_stop()
{
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
}

void periodic_timer_callback(void* arg)
{
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void convert_s_hhmmss(uint16_t sec, uint8_t *h, uint8_t *m, uint8_t *s)
{
    uint16_t tmp_sec = sec;

    *h = sec / 3600;
    tmp_sec -= *h*3600;

    *m = tmp_sec / 60;
    tmp_sec -= *m * 60;

    *s = tmp_sec;
}

#ifdef CONFIG_DISPLAY_SSD1306
esp_err_t ssd1306_update(ssd1306_canvas_t *canvas, EventBits_t uxBits)
{
    esp_err_t ret;
    char buffer[128];
    static uint8_t last_dislay_shown = 99;
    int idx = s_display_show - 1;

    ESP_LOGD(TAG, "ssd1306_update, uxBits %d", uxBits);

    if(run_periodic_timer){
        if(!periodic_timer_running){
            periodic_timer_start();
            periodic_timer_running = true;
        }
    } else {
        if(periodic_timer_running){
            periodic_timer_stop();
            periodic_timer_running = false;
        }
    }

    if(s_display_show == 0){
        if((last_dislay_shown != s_display_show)){
                ssd1306_clear_canvas(canvas, 0);
                last_dislay_shown = s_display_show;
                ret = ssd1306_refresh_gram(canvas);
                return ret;
        } else {
            return ESP_OK;
        }
    } else if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+1){
        if((last_dislay_shown != s_display_show)){
            const esp_app_desc_t *app_desc = esp_ota_get_app_description();
            uint8_t mac[6];
            ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

            ssd1306_clear_canvas(canvas, 0x00);
            snprintf(buffer, 128, "%s", app_desc->version);
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "%s", app_desc->project_name);
            ssd1306_draw_string(canvas, 0, 12, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "%s", app_desc->idf_ver);
            ssd1306_draw_string(canvas, 0, 24, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "%2X:%2X:%2X:%2X:%2X:%2X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ssd1306_draw_string(canvas, 0, 36, (const uint8_t*) buffer, 10, 1);
#ifdef CONFIG_USE_MQTT
            bool mqtt_avail = true;
#else
            bool mqtt_avail = false;
#endif
            snprintf(buffer, 128, "MQTT: %s",(mqtt_avail?"y":"n"));
            ssd1306_draw_string(canvas, 0, 48, (const uint8_t*) buffer, 10, 1);

            last_dislay_shown = s_display_show;
            return ssd1306_refresh_gram(canvas);
        } else {
            return ESP_OK;
        }
    } else if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+2){
        ssd1306_clear_canvas(canvas, 0x00);
        for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
            bool never_seen = (ble_adv_data[i].last_seen == 0);
            if(never_seen){
                snprintf(buffer, 128, "%s: %c", ble_beacon_data[i].name, '/');
            } else {
                uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_adv_data[i].last_seen)/1000000;
                uint8_t h, m, s;
                convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                snprintf(buffer, 128, "%s: %02d:%02d:%02d", ble_beacon_data[i].name, h, m, s);
            }
            ssd1306_draw_string(canvas, 0, i*10, (const uint8_t*) buffer, 10, 1);
        }
        return ssd1306_refresh_gram(canvas);
    }

#ifdef CONFIG_DISPLAY_TIME_TEST
    if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+3){
        // DISPLAY TEST FUNCTIONS
ESP_LOGE(TAG, "displaytest > 1");
            ssd1306_clear_canvas(canvas, 0x00);
            ssd1306_fill_point(canvas, 9, 0, 1);
            ssd1306_fill_point(canvas, 9, 8, 1);
            ssd1306_fill_point(canvas, 9, 16, 1);
            ssd1306_fill_point(canvas, 9, 24, 1);
ESP_LOGE(TAG, "displaytest > 2");

            // ssd1306_fill_rectangle(canvas, 1, 0, 1,  6, 1);
            // ssd1306_fill_rectangle(canvas, 2, 0, 2,  7, 1);
            // ssd1306_fill_rectangle(canvas, 3, 0, 3,  8, 1);
            // ssd1306_fill_rectangle(canvas, 4, 0, 4,  9, 1);
            // canvas->s_chDisplayBuffer[0 * canvas->w  + 5] |= 0x01;
            // canvas->s_chDisplayBuffer[0 * canvas->w  + 6] |= 0x0F;
            // canvas->s_chDisplayBuffer[0 * canvas->w  + 7] |= 0x10;
            // canvas->s_chDisplayBuffer[0 * canvas->w  + 8] |= 0xF0;
            // canvas->s_chDisplayBuffer[0 * canvas->w  + 9] |= 0xFF;

            ssd1306_fill_rectangle(canvas, 10,  0, 10,  7, 1);
            ssd1306_fill_rectangle(canvas, 11,  1, 11,  7, 1);
            ssd1306_fill_rectangle(canvas, 12,  2, 12,  7, 1);
            ssd1306_fill_rectangle(canvas, 13,  3, 13,  7, 1);
            ssd1306_fill_rectangle(canvas, 14,  4, 14,  7, 1);
            ssd1306_fill_rectangle(canvas, 15,  5, 15,  7, 1);
            ssd1306_fill_rectangle(canvas, 16,  6, 16,  7, 1);
            ssd1306_fill_rectangle(canvas, 17,  7, 17,  7, 1);
ESP_LOGE(TAG, "displaytest > 3");

            ssd1306_fill_rectangle(canvas, 20,  0, 20,  8, 1);
            ssd1306_fill_rectangle(canvas, 21,  1, 21,  9, 1);
            ssd1306_fill_rectangle(canvas, 22,  2, 22, 10, 1);
            ssd1306_fill_rectangle(canvas, 23,  3, 23, 11, 1);
            ssd1306_fill_rectangle(canvas, 24,  4, 24, 12, 1);
            ssd1306_fill_rectangle(canvas, 25,  5, 25, 13, 1);
            ssd1306_fill_rectangle(canvas, 26,  6, 26, 14, 1);
            ssd1306_fill_rectangle(canvas, 27,  7, 27, 15, 1);
            ssd1306_fill_rectangle(canvas, 28,  8, 28, 16, 1);
            ssd1306_fill_rectangle(canvas, 29,  9, 29, 17, 1);
ESP_LOGE(TAG, "displaytest > 4");

            ssd1306_fill_rectangle(canvas, 40,  0,  40, 16, 1);
            ssd1306_fill_rectangle(canvas, 41,  1,  41, 17, 1);
            ssd1306_fill_rectangle(canvas, 42,  2,  42, 18, 1);
            ssd1306_fill_rectangle(canvas, 43,  3,  43, 19, 1);
            ssd1306_fill_rectangle(canvas, 44,  4,  44, 20, 1);
            ssd1306_fill_rectangle(canvas, 45,  5,  45, 21, 1);
            ssd1306_fill_rectangle(canvas, 46,  6,  46, 22, 1);
            ssd1306_fill_rectangle(canvas, 47,  7,  47, 23, 1);
            ssd1306_fill_rectangle(canvas, 48,  8,  48, 24, 1);
            ssd1306_fill_rectangle(canvas, 49,  9,  49, 25, 1);
            ssd1306_fill_rectangle(canvas, 50,  10, 50, 26, 1);

            ssd1306_fill_point(canvas, 52, 0, 1);
            ssd1306_fill_point(canvas, 52, 8, 1);
            ssd1306_fill_point(canvas, 52, 16, 1);
            ssd1306_fill_point(canvas, 52, 24, 1);
            ssd1306_fill_point(canvas, 52, 32, 1);
ESP_LOGE(TAG, "displaytest > 5");
            ret = ssd1306_refresh_gram(canvas);
ESP_LOGE(TAG, "displaytest > 6");
    return ret;
        }
#endif // CONFIG_DISPLAY_TIME_TEST

    if(uxBits & (1 << idx)){
        ssd1306_clear_canvas(canvas, 0x00);
        snprintf(buffer, 128, "%s", ble_beacon_data[idx].name);
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
        snprintf(buffer, 128, "%5.2fC, %5.2f%%H", ble_adv_data[idx].temp, ble_adv_data[idx].humidity);
        ssd1306_draw_string(canvas, 0, 12, (const uint8_t*) buffer, 10, 1);
        snprintf(buffer, 128, "Batt %4d mV", ble_adv_data[idx].battery);
        ssd1306_draw_string(canvas, 0, 24, (const uint8_t*) buffer, 10, 1);
        snprintf(buffer, 128, "RSSI %3d dBm", ble_adv_data[idx].measured_power);
        ssd1306_draw_string(canvas, 0, 36, (const uint8_t*) buffer, 10, 1);

        last_dislay_shown = s_display_show;
        return ssd1306_refresh_gram(canvas);
    } else {
        ESP_LOGD(TAG, "ssd1306_update: not current screen to udate, exit");
        return ESP_OK;
    }

    ESP_LOGE(TAG, "ssd1306_update: this line should not be reached");
}

static void ssd1306_task(void* pvParameters)
{
    ssd1306_canvas_t *canvas = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);

    EventBits_t uxBits;
    UNUSED(uxBits);

	i2c_master_init();
	ssd1306_init();

    while (1) {
        uxBits = xEventGroupWaitBits(s_values_evg,
            UPDATE_BEAC0 | UPDATE_BEAC1 | UPDATE_BEAC2 | UPDATE_BEAC3 |
            UPDATE_BEAC4 | UPDATE_BEAC5 | UPDATE_BEAC6 | UPDATE_BEAC7 |
            UPDATE_BEAC8 | UPDATE_BEAC9 | UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGD(TAG, "ssd1306_task: uxBits = %d", uxBits);
        ssd1306_update(canvas, uxBits);
    }
    // iot_ssd1306_delete(dev, true);
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
    ble_adv_data[idx].last_seen      = esp_timer_get_time();

    xEventGroupSetBits(s_values_evg, (EventBits_t) (1 << idx));
}

bool esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len){
    bool result = false;

    ESP_LOG_BUFFER_HEXDUMP(TAG, adv_data, adv_data_len, ESP_LOG_DEBUG);

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if ( (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head, sizeof(mybeacon_common_head)))
            && (!memcmp((uint8_t*)(adv_data + sizeof(mybeacon_common_head)),
                (uint8_t*)&mybeacon_common_vendor, sizeof(mybeacon_common_vendor.proximity_uuid)))){
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet, true");
            result = true;
        } else {
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet, false");
            result = false;
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

        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");
            if(esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                ESP_LOGD(TAG, "mybeacon found");
                esp_ble_mybeacon_t *mybeacon_data = (esp_ble_mybeacon_t*)(scan_result->scan_rst.ble_adv);

                uint16_t maj      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major);
                uint16_t min      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor);
                float    temp     = SHT3_GET_TEMPERATURE_VALUE(
                                      LSB_16(mybeacon_data->mybeacon_payload.temp),
                                      MSB_16(mybeacon_data->mybeacon_payload.temp) );
                float    humidity = SHT3_GET_HUMIDITY_VALUE(
                                      LSB_16(mybeacon_data->mybeacon_payload.humidity),
                                      MSB_16(mybeacon_data->mybeacon_payload.humidity) );
                uint16_t battery  = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery);
                int16_t x         = (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.x);
                int16_t y         = (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.y);
                int16_t z         = (int16_t)ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.z);

                ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d",
                    maj, min, scan_result->scan_rst.rssi, temp, humidity, x, y, z, battery );
#ifdef CONFIG_USE_MQTT
                int msg_id = 0;
                char buffer_topic[128];
                char buffer_payload[128];

                // identifier, maj, min, sensor -> data
                // snprintf(buffer_topic, 128,  "/%s/0x%04x/x%04x/%s", "beac", maj, min, "temp");
                if( (temp < CONFIG_TEMP_LOW) || (temp > CONFIG_TEMP_HIGH) ){
                    ESP_LOGE(TAG, "temperature out of range, not send");
                } else {
                    snprintf(buffer_topic, 128,  CONFIG_MQTT_FORMAT, "beac", maj, min, "temp");
                    snprintf(buffer_payload, 128, "%.2f", temp);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                }
                if( (humidity < CONFIG_HUMIDITY_LOW) || (humidity > CONFIG_HUMIDITY_HIGH) ){
                    ESP_LOGE(TAG, "humidity out of range, not send");
                } else {
                    snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "humidity");
                    snprintf(buffer_payload, 128, "%.2f", humidity);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                }
                snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "rssi");
                snprintf(buffer_payload, 128, "%d", scan_result->scan_rst.rssi);
                msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                if( (battery < CONFIG_BATTERY_LOW) || (battery > CONFIG_BATTERY_HIGH )){
                    ESP_LOGE(TAG, "battery out of range, not send");
                } else {
                    snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "battery");
                    snprintf(buffer_payload, 128, "%d", battery);
                    msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                    ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                }
#endif // CONFIG_USE_MQTT
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

void create_timer()
{
    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            .name = "periodic"
    };

    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
}

void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    wifi_init();
    mqtt_init();
    s_values_evg = xEventGroupCreate();

    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_PUSH, button_push_cb, "PUSH");
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_release_cb, "RELEASE");

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));

    create_timer();

#ifdef CONFIG_DISPLAY_SSD1306
    xTaskCreate(&ssd1306_task, "ssd1306_task", 2048 * 2, NULL, 5, NULL);
#endif // CONFIG_DISPLAY_SSD1306

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&ble_scan_params));
}

