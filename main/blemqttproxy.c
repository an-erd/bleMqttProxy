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
#include "iot_param.h"
#include <inttypes.h>

#ifdef CONFIG_DISPLAY_SSD1306
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#endif // CONFIG_DISPLAY_SSD1306

#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

static const char* TAG = "BLEMQTTPROXY";

#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00)>>8) + (((x)&0xFF)<<8))
#define MSB_16(a) (((a) & 0xFF00) >> 8)
#define LSB_16(a) ((a) & 0x00FF)
#define UNUSED(expr) do { (void)(expr); } while (0)

#define SHT3_GET_TEMPERATURE_VALUE(temp_msb, temp_lsb) \
    (-45+(((int16_t)temp_msb << 8) | ((int16_t)temp_lsb ))*175/(float)0xFFFF)

#define SHT3_GET_HUMIDITY_VALUE(humidity_msb, humidity_lsb) \
    ((((int16_t)humidity_msb << 8) | ((int16_t)humidity_lsb))*100/(float)0xFFFF)


// IOT param
#define PARAM_NAMESPACE "blemqttproxy"
#define PARAM_KEY       "activebeac"

typedef struct {
    uint16_t active_beacon_mask;
} param_t;
param_t blemqttproxy_param = { 0 };

// BLE
const uint8_t uuid_zeros[ESP_UUID_LEN_32] = {0x00, 0x00, 0x00, 0x00};
static uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min);
static bool is_beacon_idx_active(uint16_t idx);
static void set_beacon_idx_active(uint16_t idx);
static void clear_beacon_idx_active(uint16_t idx);
static bool toggle_beacon_idx_active(uint16_t idx);
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

#define UNKNOWN_BEACON      99
static uint16_t s_active_beacon_mask = 0;

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
#define UPDATE_TEMP      (BIT10)
#define UPDATE_DISPLAY   (BIT11)
#define UPDATE_BEAC_MASK (UPDATE_BEAC0 | UPDATE_BEAC1 | UPDATE_BEAC2 | UPDATE_BEAC3 | UPDATE_BEAC4 \
                            | UPDATE_BEAC5 | UPDATE_BEAC6 | UPDATE_BEAC7 | UPDATE_BEAC8 | UPDATE_BEAC9 )
EventGroupHandle_t s_values_evg;

// Handle display to show
// 0       = off
// 1.. num = beac to show, for array minus 1
// num + 1 = app version
// num + 2 = last seen
// num + 3 = local temp

// num + 4 = display test
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

// Local sensor: DS18B20 temperatur sensor
#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
#define GPIO_DS18B20_0              (CONFIG_ONE_WIRE_GPIO)
#define OWB_MAX_DEVICES             (CONFIG_OWB_MAX_DEVICES)
#define DS18B20_RESOLUTION          (DS18B20_RESOLUTION_12_BIT)
#define LOCAL_SENSOR_SAMPLE_PERIOD  (1000)   // milliseconds
static OneWireBus *s_owb = NULL;
static OneWireBus_ROMCode s_device_rom_codes[OWB_MAX_DEVICES] = {0};
static int s_owb_num_devices = 0;
static OneWireBus_SearchState s_owb_search_state = {0};
static DS18B20_Info *s_owb_devices[OWB_MAX_DEVICES] = {0};

typedef struct  {
    float   temperature;
    int64_t last_seen;
} local_temperature_data_t;
local_temperature_data_t local_temperature_data[OWB_MAX_DEVICES] = { 0 };

#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

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

uint8_t set_next_display_show(uint8_t current_display)
{
    current_display++;

#ifndef CONFIG_LOCAL_SENSORS_TEMPERATURE
    if(current_display == CONFIG_BLE_DEVICE_COUNT_USE + 3){
        current_display++;
    }
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

#ifndef CONFIG_DISPLAY_TIME_TEST
    if(current_display == CONFIG_BLE_DEVICE_COUNT_USE + 4){
        current_display++;
    }
#endif // CONFIG_DISPLAY_TIME_TEST

    current_display %= CONFIG_BLE_DEVICE_COUNT_USE + 5;

    return current_display;
}

void handle_long_button_push(uint8_t current_display)
{
    if( (current_display > 0) && (current_display < (CONFIG_BLE_DEVICE_COUNT_USE+1))){
        // beacon detail screen shown
        toggle_beacon_idx_active(current_display - 1);
    }
}

void button_release_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    if(esp_timer_get_time() < time_button_long_press){
        s_display_show = set_next_display_show(s_display_show);
    } else {
        // s_display_show = 0;
        handle_long_button_push(s_display_show);
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
#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
    } else if(s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+3){
        // display local sensor temperature, update with each value update
        run_periodic_timer = false;
        xEventGroupSetBits(s_values_evg, UPDATE_TEMP);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
#ifdef CONFIG_DISPLAY_TIME_TEST
    } else if(s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+4){
        // Display test functions
        run_periodic_timer = false;
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
#endif // CONFIG_DISPLAY_TIME_TEST
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
    periodic_timer_running = true;
}

void periodic_timer_stop()
{
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
    periodic_timer_running = false;
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
void draw_pagenumber(ssd1306_canvas_t *canvas, uint8_t nr_act, uint8_t nr_total)
{
    char buffer2[5];
    snprintf(buffer2, 5, "%d/%d", nr_act, nr_total);
    ssd1306_draw_string_8x8(canvas, 128-3*8, 7, (const uint8_t*) buffer2);
}

esp_err_t ssd1306_update(ssd1306_canvas_t *canvas, EventBits_t uxBits)
{
    esp_err_t ret;
    char buffer[128], buffer2[32];
    static uint8_t last_dislay_shown = 99;
    int idx = s_display_show - 1;

    ESP_LOGD(TAG, "ssd1306_update, uxBits %d", uxBits);

    if(run_periodic_timer){
        if(!periodic_timer_running){
            periodic_timer_start();
        }
    } else {
        if(periodic_timer_running){
            periodic_timer_stop();
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
            ssd1306_draw_string(canvas, 0, 11, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "%s", app_desc->idf_ver);
            ssd1306_draw_string(canvas, 0, 22, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "%2X:%2X:%2X:%2X:%2X:%2X",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            ssd1306_draw_string(canvas, 0, 33, (const uint8_t*) buffer, 10, 1);
#ifdef CONFIG_USE_MQTT
            bool mqtt_avail = true;
#else
            bool mqtt_avail = false;
#endif
            snprintf(buffer, 128, "MQTT: %s", (mqtt_avail ? "y" : "n"));
            ssd1306_draw_string(canvas, 0, 44, (const uint8_t*) buffer, 10, 1);

            itoa(s_active_beacon_mask, buffer2, 2);
            int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
            if(!num_lead_zeros){
                snprintf(buffer, 128, "Act:  %s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
            } else {
                snprintf(buffer, 128, "Act:  %0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
            }
            ssd1306_draw_string(canvas, 0, 55, (const uint8_t*) buffer, 10, 1);

            last_dislay_shown = s_display_show;
            return ssd1306_refresh_gram(canvas);
        } else {
            return ESP_OK;
        }
    } else if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+2){
        uint8_t num_act_beac = 0, num_pages;
        uint8_t cur_page = 1;
        for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
            if(is_beacon_idx_active(i)){
                num_act_beac++;
            }
        }
        num_pages = num_act_beac / 6 + (num_act_beac % 6 ? 1:0) + (!num_act_beac ? 1 : 0);

        ssd1306_clear_canvas(canvas, 0x00);
        if(!num_act_beac){
            snprintf(buffer, 128, "no active beacon");
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
        } else {
            snprintf(buffer, 128, "beacon last seen");
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
            int line = 1;
            for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
                if(is_beacon_idx_active(i)){
                    bool never_seen = (ble_adv_data[i].last_seen == 0);
                    if(never_seen){
                        snprintf(buffer, 128, "%s: %c", ble_beacon_data[i].name, '/');
                    } else {
                        uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_adv_data[i].last_seen)/1000000;
                        uint8_t h, m, s;
                        convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                        snprintf(buffer, 128, "%s: %02d:%02d:%02d", ble_beacon_data[i].name, h, m, s);
                    }
                    ssd1306_draw_string(canvas, 0, line*10, (const uint8_t*) buffer, 10, 1);
                    line++;
                }
            }
        }
        draw_pagenumber(canvas, cur_page, num_pages);
        return ssd1306_refresh_gram(canvas);
#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
    } else if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+3){
        ssd1306_clear_canvas(canvas, 0x00);
        snprintf(buffer, 128, "no local temperature");
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
        draw_pagenumber(canvas, 1, 1);
        return ssd1306_refresh_gram(canvas);
#endif
    }

#ifdef CONFIG_DISPLAY_TIME_TEST
    if (s_display_show == CONFIG_BLE_DEVICE_COUNT_USE+4){
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
        if(is_beacon_idx_active(idx)){
            snprintf(buffer, 128, "%5.2fC, %5.2f%%H", ble_adv_data[idx].temp, ble_adv_data[idx].humidity);
            ssd1306_draw_string(canvas, 0, 12, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "Batt %4d mV", ble_adv_data[idx].battery);
            ssd1306_draw_string(canvas, 0, 24, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "RSSI  %3d dBm", ble_adv_data[idx].measured_power);
            ssd1306_draw_string(canvas, 0, 36, (const uint8_t*) buffer, 10, 1);
        } else {
            snprintf(buffer, 128, "  -  C,   -  %%H");
            ssd1306_draw_string(canvas, 0, 12, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "Batt   -  mV");
            ssd1306_draw_string(canvas, 0, 24, (const uint8_t*) buffer, 10, 1);
            snprintf(buffer, 128, "RSSI   -  dBm");
            ssd1306_draw_string(canvas, 0, 36, (const uint8_t*) buffer, 10, 1);
            }
        snprintf(buffer, 128, "active: %s",(is_beacon_idx_active(idx)?"y":"n"));
        ssd1306_draw_string(canvas, 0, 48, (const uint8_t*) buffer, 10, 1);

        draw_pagenumber(canvas, idx+1, CONFIG_BLE_DEVICE_COUNT_USE);

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
            UPDATE_BEAC_MASK | UPDATE_TEMP | UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
        ESP_LOGD(TAG, "ssd1306_task: uxBits = %d", uxBits);
        ssd1306_update(canvas, uxBits);
    }
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

    return UNKNOWN_BEACON;
}

bool is_beacon_idx_active(uint16_t idx)
{
    return (s_active_beacon_mask & (1 << idx) );
}

void set_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask |= (1 << idx);
}

void clear_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask &= ~(1 << idx);
}

bool toggle_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask ^= (1 << idx);

    return s_active_beacon_mask & (1 << idx);
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
                uint8_t  idx      = beacon_maj_min_to_idx(maj, min);

                if( (idx != UNKNOWN_BEACON) && (!is_beacon_idx_active(idx)) ){
                    if(scan_result->scan_rst.rssi > CONFIG_PROXIMITY_RSSI_THRESHOLD) {
                        ESP_LOGI(TAG, "Announcing new mybeacon (0x%04x%04x), idx %d, RSSI %d", maj, min, idx,
                            scan_result->scan_rst.rssi);
                        set_beacon_idx_active(idx);
                    } else {
                        ESP_LOGD(TAG, "mybeacon not active, not close enough (0x%04x%04x), idx %d, RSSI %d",
                            maj, min, idx, scan_result->scan_rst.rssi);
                        break;
                    }
                }

                float    temp     = SHT3_GET_TEMPERATURE_VALUE(
                                      LSB_16(mybeacon_data->mybeacon_payload.temp),
                                      MSB_16(mybeacon_data->mybeacon_payload.temp) );
                float    humidity = SHT3_GET_HUMIDITY_VALUE(
                                      LSB_16(mybeacon_data->mybeacon_payload.humidity),
                                      MSB_16(mybeacon_data->mybeacon_payload.humidity) );
                uint16_t battery  = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery);
                int16_t x         = (int16_t)(mybeacon_data->mybeacon_payload.x);
                int16_t y         = (int16_t)(mybeacon_data->mybeacon_payload.y);
                int16_t z         = (int16_t)(mybeacon_data->mybeacon_payload.z);

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

#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
void init_owb_tempsensor(){
    // Create a 1-Wire bus, using the RMT timeslot driver
    owb_rmt_driver_info rmt_driver_info;
    s_owb = owb_rmt_initialize(&rmt_driver_info, GPIO_DS18B20_0, RMT_CHANNEL_1, RMT_CHANNEL_0);
    owb_use_crc(s_owb, true);  // enable CRC check for ROM code

    // Find all connected devices
    ESP_LOGI(TAG, "init_owb_tempsensor: find devices");

    bool found = false;
    owb_search_first(s_owb, &s_owb_search_state, &found);
    while (found)
    {
        char rom_code_s[17];
        owb_string_from_rom_code(s_owb_search_state.rom_code, rom_code_s, sizeof(rom_code_s));
        ESP_LOGI(TAG, "  %d : %s", s_owb_num_devices, rom_code_s);
        s_device_rom_codes[s_owb_num_devices] = s_owb_search_state.rom_code;
        ++s_owb_num_devices;
        owb_search_next(s_owb, &s_owb_search_state, &found);
    }
    ESP_LOGI(TAG, "Found device(s): %d", s_owb_num_devices);

    if (s_owb_num_devices == 1)
    {
        // For a single device only:
        OneWireBus_ROMCode rom_code;
        owb_status status = owb_read_rom(s_owb, &rom_code);
        if (status == OWB_STATUS_OK)
        {
            char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
            owb_string_from_rom_code(rom_code, rom_code_s, sizeof(rom_code_s));
            ESP_LOGI(TAG, "Single device %s present", rom_code_s);
        }
        else
        {
        }
    }
    else
    {
        ESP_LOGE(TAG, "init_owb_tempsensor: found %d devices, expected 1 device", s_owb_num_devices);
        // // Search for a known ROM code (LSB first):
        // // For example: 0x1502162ca5b2ee28
        // OneWireBus_ROMCode known_device = {
        //     .fields.family = { 0x28 },
        //     .fields.serial_number = { 0xee, 0xb2, 0xa5, 0x2c, 0x16, 0x02 },
        //     .fields.crc = { 0x15 },
        // };
        // char rom_code_s[OWB_ROM_CODE_STRING_LENGTH];
        // owb_string_from_rom_code(known_device, rom_code_s, sizeof(rom_code_s));
        // bool is_present = false;

        // owb_status search_status = owb_verify_rom(s_owb, known_device, &is_present);
        // if (search_status == OWB_STATUS_OK)
        // {
        //     printf("Device %s is %s", rom_code_s, is_present ? "present" : "not present");
        // }
        // else
        // {
        //     printf("An error occurred searching for known device: %d", search_status);
        // }
    }

    // Create DS18B20 devices on the 1-Wire bus
    for (int i = 0; i < s_owb_num_devices; ++i)
    {
        DS18B20_Info * ds18b20_info = ds18b20_malloc();  // heap allocation
        s_owb_devices[i] = ds18b20_info;

        if (s_owb_num_devices == 1)
        {
            ESP_LOGI(TAG,"Single device optimisations enabled");
            ds18b20_init_solo(ds18b20_info, s_owb);
        }
        else
        {
            ds18b20_init(ds18b20_info, s_owb, s_device_rom_codes[i]); // associate with bus and device
        }
        ds18b20_use_crc(ds18b20_info, true);           // enable CRC check for temperature readings
        ds18b20_set_resolution(ds18b20_info, DS18B20_RESOLUTION);
    }
}

void cleanup_owb_tempsensor(){
    // clean up dynamically allocated data
    for (int i = 0; i < s_owb_num_devices; ++i)
    {
        ds18b20_free(&s_owb_devices[i]);
    }
    owb_uninitialize(s_owb);
}

void update_local_temp_data(uint8_t num_devices, float *readings)
{
    for(int i = 0; i < num_devices; i++){
        local_temperature_data[i].temperature = readings[i];
        local_temperature_data[i].last_seen   = esp_timer_get_time();
    }

    xEventGroupSetBits(s_values_evg, UPDATE_TEMP);
}

static void localsensor_task(void* pvParameters)
{
    int errors_count[OWB_MAX_DEVICES] = {0};
    int sample_count = 0;
    if (s_owb_num_devices > 0){
        TickType_t last_wake_time = xTaskGetTickCount();

        while (1)
        {
            last_wake_time = xTaskGetTickCount();

            ds18b20_convert_all(s_owb);

            // In this application all devices use the same resolution,
            // so use the first device to determine the delay
            ds18b20_wait_for_conversion(s_owb_devices[0]);

            // Read the results immediately after conversion otherwise it may fail
            float readings[OWB_MAX_DEVICES] = { 0 };
            DS18B20_ERROR errors[OWB_MAX_DEVICES] = { 0 };

            for (int i = 0; i < s_owb_num_devices; ++i)
            {
                errors[i] = ds18b20_read_temp(s_owb_devices[i], &readings[i]);
            }

            ESP_LOGI(TAG, "Temperature readings (degrees C): sample %d", ++sample_count);
            for (int i = 0; i < s_owb_num_devices; ++i){
                if (errors[i] != DS18B20_OK){
                    ++errors_count[i];
                }
                ESP_LOGI(TAG, "  %d: %.1f    %d errors", i, readings[i], errors_count[i]);
            }

            update_local_temp_data(s_owb_num_devices, readings);

            vTaskDelayUntil(&last_wake_time, LOCAL_SENSOR_SAMPLE_PERIOD / portTICK_PERIOD_MS);
        }
    }

    vTaskDelete(NULL);
}

#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

static void initialize_nvs()
{
    esp_err_t ret;

    // Initialize NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
}

static esp_err_t read_blemqttproxy_param()
{
    esp_err_t ret;
    uint16_t mask = 0xFFFF;

    ret = iot_param_load(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param);
    if(ret == ESP_OK){
        ESP_LOGI(TAG, "read_blemqttproxy_param: read param ok, beacon mask %u", blemqttproxy_param.active_beacon_mask);
    } else {
        ESP_LOGE(TAG, "read_blemqttproxy_param: read param failed, ret = %d, initialize and save to NVS", ret);
        blemqttproxy_param.active_beacon_mask = CONFIG_ACTIVE_BLE_DEVICE_MASK;
        ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    }

    mask >>= 16 - CONFIG_BLE_DEVICE_COUNT_USE; // len = 16 bit, only use configured dev to use
    s_active_beacon_mask = blemqttproxy_param.active_beacon_mask & mask;

    return ret;
}

void app_main()
{
    // NVS initialization and beacon mask retrieval
    initialize_nvs();
    ESP_ERROR_CHECK(read_blemqttproxy_param());

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

#ifdef CONFIG_LOCAL_SENSORS_TEMPERATURE
    init_owb_tempsensor();
    xTaskCreate(&localsensor_task, "localsensor_task", 2048 * 2, NULL, 5, NULL);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&ble_scan_params));
}

