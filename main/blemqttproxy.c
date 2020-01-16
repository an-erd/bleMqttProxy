#include <sdkconfig.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event_loop.h"
#include "esp_assert.h"
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
#include "splashscreen.h"
#endif // CONFIG_DISPLAY_SSD1306

#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
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

static esp_err_t save_blemqttproxy_param();


// BLE
const uint8_t uuid_zeros[ESP_UUID_LEN_32] = {0x00, 0x00, 0x00, 0x00};
static uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min);
static uint8_t num_active_beacon();
static uint8_t first_active_beacon();
static bool is_beacon_idx_active(uint16_t idx);
static void set_beacon_idx_active(uint16_t idx);
static void clear_beacon_idx_active(uint16_t idx);
static bool toggle_beacon_idx_active(uint16_t idx);
static void persist_active_beacon_mask();
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
    int64_t     mqtt_last_send;
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
typedef enum {
    BEACON_V3           = 0,    // ble_bacon v3 adv
    BEACON_V4,                  // only ble_beacon v4 adv
    BEACON_V4_SR,                // ble_beacon v4 adv+sr
    UNKNOWN_BEACON      = 99
} beacon_type_t;

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
}__attribute__((packed)) esp_ble_mybeacon_v3_t;

esp_ble_mybeacon_head_t mybeacon_common_head_v3 = {
    .flags = {0x02, 0x01, 0x04},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x1502
};

esp_ble_mybeacon_head_t mybeacon_common_head_v4 = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x13,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x0700
};

esp_ble_mybeacon_vendor_t mybeacon_common_vendor_v3 = {
    .proximity_uuid ={CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4},
};

static uint16_t s_active_beacon_mask = 0;

// Display
#define UPDATE_DISPLAY      (BIT0)
EventGroupHandle_t s_values_evg;

#define BEAC_PER_PAGE_LASTSEEN  5

typedef enum {
    EMPTY_SCREEN        = 0,
    SPLASH_SCREEN,
    BEACON_SCREEN,
    LASTSEEN_SCREEN,
    LOCALTEMP_SCREEN,
    APPVERSION_SCREEN,
    MAX_SCREEN_NUM,
    UNKNOWN_SCREEN      = 99
} display_screen_t;

// status screen values
typedef struct {
    display_screen_t    current_screen;
    display_screen_t    screen_to_show;
    // Beacon
    uint8_t             current_beac;
    uint8_t             beac_to_show;   // 0..CONFIG_BLE_DEVICE_COUNT_USE-1
    // last seen
    uint8_t             lastseen_page_to_show;
    uint8_t             num_last_seen_pages;
    // local Temperature sensor
    uint8_t             localtemp_to_show;
    uint8_t             num_localtemp_pages;
    // enable/disable button
    bool                button_enabled;
} display_status_t;

static display_status_t s_display_status = {
    .current_screen = UNKNOWN_SCREEN,
    .screen_to_show = SPLASH_SCREEN,
    .button_enabled = false
};

// Wifi
static EventGroupHandle_t s_wifi_evg;
const static int CONNECTED_BIT = BIT0;

// MQTT
static esp_mqtt_client_handle_t s_client;
static EventGroupHandle_t s_mqtt_evg;
const static int MQTT_CONNECTED_BIT = BIT0;

// Timer

// The Oneshot timer is used for displaying the splash screen or the idle timer (to switch to empty screen after inactivity)

#define SPLASH_SCREEN_TIMER_DURATION    2500000     // 2.5 sec
#define IDLE_TIMER_DURATION             (CONFIG_DISPLAY_IDLE_TIMER * 1000000)

typedef enum {
    TIMER_NO_USAGE = 0,
    TIMER_SPLASH_SCREEN,
    TIMER_IDLE_TIMER
} oneshot_timer_usage_t;

static oneshot_timer_usage_t oneshot_timer_usage = { TIMER_NO_USAGE };
static esp_timer_handle_t oneshot_timer;
static void oneshot_timer_callback(void* arg);

static volatile bool idle_timer_running = false;    // status of the timer
static volatile bool run_idle_timer = false;        // start/stop idle timer, set during cb , will be handled in ssd1306_update
static volatile bool run_idle_timer_touch = false;  // touch the idle timer, will be handled in ssd1306_update
static volatile bool show_empty_screen = false;     // switch to empty screen as idle timer action, will be handled in ssd1306_update

static void idle_timer_start();
static void idle_timer_stop();
static void idle_timer_touch();
static bool idle_timer_is_running();

// The periodic timer is used to update the display regulary, e.g. for displaying last seen/send screen

#define UPDATE_LAST_SEEN_INTERVAL       250000      // 4 Hz

static esp_timer_handle_t periodic_timer;
static volatile bool periodic_timer_running = false;
static volatile bool run_periodic_timer = false;
static void periodic_timer_callback(void* arg);
static void periodic_timer_start();
static void periodic_timer_stop();
static bool periodic_timer_is_running();

// Watchdog timer / WDT
static esp_timer_handle_t periodic_wdt_timer;
static void periodic_wdt_timer_callback(void* arg);
static void periodic_wdt_timer_start();
#define UPDATE_ESP_RESTART          (BIT0)
#define UPDATE_ESP_MQTT_RESTART     (BIT1)
static uint8_t esp_restart_mqtt_beacon_to_take = UNKNOWN_BEACON;
static EventGroupHandle_t s_wdt_evg;
#define WDT_TIMER_DURATION              (CONFIG_WDT_OWN_INTERVAL * 1000000)

// Local sensor: DS18B20 temperatur sensor
#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
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

#if CONFIG_DISABLE_BUTTON_HEADLESS==0
void button_push_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    if(!s_display_status.button_enabled){
        ESP_LOGD(TAG, "button_push_cb: button not enabled");
        return;
    }

    time_button_long_press = esp_timer_get_time() + CONFIG_LONG_PRESS_TIME * 1000;

    ESP_LOGD(TAG, "button_push_cb");
}
#endif // CONFIG_DISABLE_BUTTON_HEADLESS

void set_next_display_show()
{
    switch(s_display_status.current_screen){
        case SPLASH_SCREEN:
            s_display_status.screen_to_show = EMPTY_SCREEN;
            s_display_status.button_enabled = true;
            break;
        case EMPTY_SCREEN:
            s_display_status.screen_to_show = BEACON_SCREEN;
            s_display_status.current_beac = UNKNOWN_BEACON;
            s_display_status.beac_to_show = 0;
            idle_timer_start();
            break;
        case BEACON_SCREEN:
            if(s_display_status.beac_to_show < CONFIG_BLE_DEVICE_COUNT_USE - 1){
                s_display_status.beac_to_show++;
            } else {
                s_display_status.current_beac = UNKNOWN_BEACON;
                s_display_status.screen_to_show = LASTSEEN_SCREEN;
                s_display_status.lastseen_page_to_show = 1;
            }
            break;
        case LASTSEEN_SCREEN:
            if(s_display_status.lastseen_page_to_show < s_display_status.num_last_seen_pages){
                s_display_status.lastseen_page_to_show++;
            } else {
#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
                if(s_owb_num_devices >= CONFIG_MENU_MIN_LOCAL_SENSOR){
                    s_display_status.screen_to_show = LOCALTEMP_SCREEN;
                    s_display_status.localtemp_to_show = 1;
                } else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
                {
                    // skip empty local temperature screen
                    s_display_status.screen_to_show = APPVERSION_SCREEN;
                }
            }
            break;
        case LOCALTEMP_SCREEN:
#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
            if(s_display_status.localtemp_to_show < s_display_status.num_localtemp_pages){
                s_display_status.localtemp_to_show++;
            } else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
            {
                s_display_status.screen_to_show = APPVERSION_SCREEN;
            }
            break;
        case APPVERSION_SCREEN:
            idle_timer_stop();
            s_display_status.screen_to_show = EMPTY_SCREEN;
            break;
        default:
            ESP_LOGE(TAG, "set_next_display_show: unhandled switch-case");
            break;
    }
}

#if CONFIG_DISABLE_BUTTON_HEADLESS==0
void handle_long_button_push()
{
    switch(s_display_status.current_screen){
        case EMPTY_SCREEN:
            break;
        case BEACON_SCREEN:
            toggle_beacon_idx_active(s_display_status.beac_to_show);
            break;
        case LASTSEEN_SCREEN:
            break;
        case LOCALTEMP_SCREEN:
            break;
        case APPVERSION_SCREEN:
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }
}

void button_release_cb(void* arg)
{
    char* pstr = (char*) arg;
    UNUSED(pstr);

    if(!s_display_status.button_enabled){
        ESP_LOGD(TAG, "button_release_cb: button not enabled");
        return;
    }

    run_idle_timer_touch = true;

    ESP_LOGD(TAG, "button_release_cb: s_display_status.current_screen %d screen_to_show %d >",
        s_display_status.current_screen, s_display_status.screen_to_show);

    if(esp_timer_get_time() < time_button_long_press){
        set_next_display_show();
    } else {
        handle_long_button_push();
    }

    switch(s_display_status.screen_to_show){
        case EMPTY_SCREEN:
            run_periodic_timer = false;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case BEACON_SCREEN:
            run_periodic_timer = true;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case LASTSEEN_SCREEN:
            run_periodic_timer = true;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case LOCALTEMP_SCREEN:
            run_periodic_timer = true;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case APPVERSION_SCREEN:
            run_periodic_timer = true;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }

    ESP_LOGD(TAG, "button_release_cb: s_display_status.current_screen %d screen_to_show %d <",
        s_display_status.current_screen, s_display_status.screen_to_show);
}
#endif // CONFIG_DISABLE_BUTTON_HEADLESS

__attribute__((unused)) void periodic_timer_start()
{
    periodic_timer_running = true;
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, UPDATE_LAST_SEEN_INTERVAL));
}

__attribute__((unused)) void periodic_timer_stop()
{
    periodic_timer_running = false;
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
}

__attribute__((unused)) bool periodic_timer_is_running()
{
    ESP_LOGD(TAG, "periodic_timer_is_running(), %d", periodic_timer_running);

    return periodic_timer_running;
}

void periodic_timer_callback(void* arg)
{
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void oneshot_timer_callback(void* arg)
{
    oneshot_timer_usage_t usage = *(oneshot_timer_usage_t *)arg;
    ESP_LOGD(TAG, "oneshot_timer_callback: usage %d, oneshot_timer_usage %d", usage, oneshot_timer_usage);  // TODO DEBUG

    switch(usage){
        case TIMER_NO_USAGE:
            ESP_LOGE(TAG, "oneshot_timer_callback: TIMER_NO_USAGE, should not happen");
            break;
        case TIMER_SPLASH_SCREEN:
            // currently only move from SPLASH -> EMPTY screen and enable button
            set_next_display_show();
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case TIMER_IDLE_TIMER:
            show_empty_screen = true;
            run_idle_timer = false;
            run_periodic_timer = false;
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        default:
            ESP_LOGE(TAG, "oneshot_timer_callback: unhandled usage, should not happen");
            break;
    }

    oneshot_timer_usage = TIMER_NO_USAGE;
}

void idle_timer_start(){
    assert(oneshot_timer_usage == TIMER_NO_USAGE);
    if(!IDLE_TIMER_DURATION)
        return;
    ESP_LOGD(TAG, "idle_timer_start()");
    oneshot_timer_usage = TIMER_IDLE_TIMER;
    idle_timer_running = true;
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, IDLE_TIMER_DURATION));
}

void idle_timer_stop(){
    if(!IDLE_TIMER_DURATION)
        return;
    ESP_LOGI(TAG, "idle_timer_stop()");
    assert(oneshot_timer_usage == TIMER_IDLE_TIMER);
    oneshot_timer_usage = TIMER_NO_USAGE;
    idle_timer_running = false;
    ESP_ERROR_CHECK(esp_timer_stop(oneshot_timer));
}

void idle_timer_touch(){
    if(!IDLE_TIMER_DURATION)
        return;
    ESP_LOGD(TAG, "idle_timer_touch()");
    assert(oneshot_timer_usage == TIMER_IDLE_TIMER);
    ESP_ERROR_CHECK(esp_timer_stop(oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, IDLE_TIMER_DURATION));
}

bool idle_timer_is_running(){
    ESP_LOGD(TAG, "idle_timer_is_running(), %d", (oneshot_timer_usage == TIMER_IDLE_TIMER));
    ESP_LOGD(TAG, "idle_timer_is_running %d, usage %d", idle_timer_running, oneshot_timer_usage);

    return idle_timer_running;
}

__attribute__((unused)) void periodic_wdt_timer_start(){
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_wdt_timer, WDT_TIMER_DURATION));
}

void periodic_wdt_timer_callback(void* arg)
{
    EventBits_t uxSet, uxReturn;

    ESP_LOGD(TAG, "periodic_wdt_timer_callback(): >>");
    uint8_t beacon_to_take = first_active_beacon();
    ESP_LOGD(TAG, "periodic_wdt_timer_callback: first active beac %d", beacon_to_take);
    if(beacon_to_take == UNKNOWN_BEACON){
        ESP_LOGD(TAG, "periodic_wdt_timer_callback: no active beac <<");
        return;
    }

    uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_adv_data[beacon_to_take].last_seen)/1000000;
    ESP_LOGD(TAG, "periodic_wdt_timer_callback(): last_seen_sec_gone = %d", last_seen_sec_gone);
    if(last_seen_sec_gone > CONFIG_WDT_LAST_SEEN_THRESHOLD){

        uxReturn = xEventGroupWaitBits(s_mqtt_evg, CONNECTED_BIT, false, true, 0);
        bool mqtt_connected = uxReturn & CONNECTED_BIT;

        uxReturn = xEventGroupWaitBits(s_wifi_evg, CONNECTED_BIT, false, true, 0);
        bool wifi_connected = uxReturn & CONNECTED_BIT;

        ESP_LOGE(TAG, "periodic_wdt_timer_callback: last seen > threshold: %d sec, WIFI: %s, MQTT (enabled/onnected): %s/%s", last_seen_sec_gone,
            (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));

        uxSet = 0;
        if(CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD){
            uxSet |= UPDATE_ESP_RESTART;
        }
        if(CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT){
            uxSet |= UPDATE_ESP_MQTT_RESTART;
        }
        if(uxSet){
            ESP_LOGE(TAG, "periodic_wdt_timer_callback: reboot/mqtt flag set: %d", uxSet);
            esp_restart_mqtt_beacon_to_take = beacon_to_take;
            xEventGroupSetBits(s_wdt_evg, uxSet);
        }
    }
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

void convert_s_mmss(uint16_t sec, uint8_t *m, uint8_t *s)
{
    uint16_t tmp_sec = sec;

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

esp_err_t ssd1306_update(ssd1306_canvas_t *canvas)
{
    esp_err_t ret;
    UNUSED(ret);
    char buffer[128], buffer2[32];
    EventBits_t uxReturn;

    ESP_LOGD(TAG, "ssd1306_update >, run_periodic_timer %d, run_idle_timer_touch %d, periodic_timer_running %d",
        run_periodic_timer, run_idle_timer_touch, periodic_timer_running);
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));

    if(run_periodic_timer){
        if(!periodic_timer_running){
            periodic_timer_start();
        }
    } else {
        if(periodic_timer_running){
            periodic_timer_stop();
        }
    }

    if( run_idle_timer_touch ){
        run_idle_timer_touch = false;
        ESP_LOGD(TAG, "ssd1306_update idle_timer_is_running() = %d", idle_timer_is_running());

        if(idle_timer_is_running()){
            idle_timer_touch();
        }
    }

    if (show_empty_screen){
        show_empty_screen = false;
        s_display_status.current_screen = UNKNOWN_SCREEN;
        s_display_status.screen_to_show = EMPTY_SCREEN;
    }

    ESP_LOGD(TAG, "ssd1306_update current_screen %d, screen_to_show %d", s_display_status.current_screen, s_display_status.screen_to_show);

    switch(s_display_status.screen_to_show){

        case EMPTY_SCREEN:
            ESP_LOGD(TAG, "ssd1306_update current_screen %d, screen_to_show %d", s_display_status.current_screen, s_display_status.screen_to_show);
            if((s_display_status.current_screen != s_display_status.screen_to_show)){
                ESP_LOGD(TAG, "ssd1306_update next command  ssd1306_clear_canvas");
                ssd1306_clear_canvas(canvas, 0);
                s_display_status.current_screen = s_display_status.screen_to_show;
                return ssd1306_refresh_gram(canvas);
            } else {
                return ESP_OK;
            }
            break;

        case SPLASH_SCREEN:
            memcpy((void *) canvas->s_chDisplayBuffer, (void *) blemqttproxy_splash1, canvas->w * canvas->h);
            s_display_status.current_screen = s_display_status.screen_to_show;
            return ssd1306_refresh_gram(canvas);
            break;

        case BEACON_SCREEN:{
            int idx = s_display_status.beac_to_show;
            if( (s_display_status.current_screen != s_display_status.screen_to_show)
                || (s_display_status.current_beac != s_display_status.beac_to_show) ){  // TODO
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

                s_display_status.current_screen = s_display_status.screen_to_show;
                return ssd1306_refresh_gram(canvas);
            } else {
                ESP_LOGD(TAG, "ssd1306_update: not current screen to udate, exit");
                return ESP_OK;
            }
            break;
        }

        case LASTSEEN_SCREEN:{
            uint8_t num_act_beac = num_active_beacon();
            s_display_status.num_last_seen_pages = num_act_beac / BEAC_PER_PAGE_LASTSEEN
                + (num_act_beac % BEAC_PER_PAGE_LASTSEEN ? 1:0) + (!num_act_beac ? 1 : 0);

            if(s_display_status.lastseen_page_to_show > s_display_status.num_last_seen_pages){
                // due to deannouncment by "touching" the beacon - TODO
                s_display_status.lastseen_page_to_show = s_display_status.num_last_seen_pages;
            }

            ssd1306_clear_canvas(canvas, 0x00);

            snprintf(buffer, 128, "Last seen/send:");
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
            if(!num_act_beac){
                s_display_status.lastseen_page_to_show = 1;
                snprintf(buffer, 128, "No active beacon!");
                ssd1306_draw_string(canvas, 0, 10, (const uint8_t*) buffer, 10, 1);
            } else {
                int skip  = (s_display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
                int line = 1;
                for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
                    if(is_beacon_idx_active(i)){
                        if(skip){
                            skip--;
                        } else {
                            bool never_seen = (ble_adv_data[i].last_seen == 0);
                            if(never_seen){
                                snprintf(buffer, 128, "%s: %c", ble_beacon_data[i].name, '/');
                            } else {
                                uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_adv_data[i].last_seen)/1000000;
                                uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_adv_data[i].mqtt_last_send)/1000000;
                                uint8_t m, s, mq, sq;
                                convert_s_mmss(last_seen_sec_gone, &m, &s);
                                convert_s_mmss(mqtt_last_send_sec_gone, &mq, &sq);
                                if(m>99){
                                    snprintf(buffer, 128, "%s: %c", ble_beacon_data[i].name, '>');
                                } else {
                                    snprintf(buffer, 128, "%s: %02d:%02d %02d:%02d", ble_beacon_data[i].name, m, s, mq, sq);
                                }
                            }
                            ssd1306_draw_string(canvas, 0, line*10, (const uint8_t*) buffer, 10, 1);
                            if(BEAC_PER_PAGE_LASTSEEN == line++){
                                break;
                            };
                        }
                    }
                }
            }
            draw_pagenumber(canvas, s_display_status.lastseen_page_to_show, s_display_status.num_last_seen_pages);
            s_display_status.current_screen = s_display_status.screen_to_show;
            return ssd1306_refresh_gram(canvas);
            break;
        }

        case LOCALTEMP_SCREEN:
#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
            ssd1306_clear_canvas(canvas, 0x00);
            if(s_owb_num_devices == 0){
                snprintf(buffer, 128, "No local temperature!");
                ssd1306_draw_string(canvas, 0, 0, (const uint8_t*) buffer, 10, 1);
            } else {
                // localtemp_to_show
            }
            draw_pagenumber(canvas, s_display_status.localtemp_to_show, s_display_status.num_localtemp_pages);

            s_display_status.current_screen = s_display_status.screen_to_show;
            return ssd1306_refresh_gram(canvas);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
            break;

        case APPVERSION_SCREEN:
            if((s_display_status.current_screen != s_display_status.screen_to_show)){
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

                uxReturn = xEventGroupWaitBits(s_mqtt_evg, CONNECTED_BIT, false, true, 0);
                bool mqtt_connected = uxReturn & CONNECTED_BIT;

                uxReturn = xEventGroupWaitBits(s_wifi_evg, CONNECTED_BIT, false, true, 0);
                bool wifi_connected = uxReturn & CONNECTED_BIT;

                snprintf(buffer, 128, "WIFI: %s, MQTT: %s/%s", (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
                ssd1306_draw_string(canvas, 0, 44, (const uint8_t*) buffer, 10, 1);

                itoa(s_active_beacon_mask, buffer2, 2);
                int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
                if(!num_lead_zeros){
                    snprintf(buffer, 128, "Act:  %s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
                } else {
                    snprintf(buffer, 128, "Act:  %0*d%s (%d..1)", num_lead_zeros, 0, buffer2,   CONFIG_BLE_DEVICE_COUNT_USE);
                }
                ssd1306_draw_string(canvas, 0, 55, (const uint8_t*) buffer, 10, 1);

                s_display_status.current_screen = s_display_status.screen_to_show;
                return ssd1306_refresh_gram(canvas);
            } else {
                return ESP_OK;
            }
            break;
        default:
            ESP_LOGE(TAG, "unhandled ssd1306_update screen");
            break;
    }

    ESP_LOGE(TAG, "ssd1306_update: this line should not be reached");
    return ESP_FAIL;
}

static void ssd1306_task(void* pvParameters)
{
    ssd1306_canvas_t *canvas = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);

    EventBits_t uxBits;
    UNUSED(uxBits);

	i2c_master_init();
	ssd1306_init();

    while (1) {
        uxBits = xEventGroupWaitBits(s_values_evg, UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
        ssd1306_update(canvas);
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
            xEventGroupSetBits(s_wifi_evg, CONNECTED_BIT);
            ESP_LOGI(TAG, "wifi_event_handler: SYSTEM_EVENT_STA_GOT_IP");
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_evg, CONNECTED_BIT);
            ESP_LOGI(TAG, "wifi_event_handler: SYSTEM_EVENT_STA_DISCONNECTED");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void)
{
    tcpip_adapter_init();
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
    // WiFi.setSleepMode(WIFI_NONE_SLEEP);
    ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    xEventGroupWaitBits(s_wifi_evg, CONNECTED_BIT, false, true, portMAX_DELAY);
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
            xEventGroupSetBits(s_mqtt_evg, CONNECTED_BIT);
            ESP_LOGI(TAG, "mqtt_event_handler: MQTT_EVENT_CONNECTED");
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "MQTT_EVENT_DISCONNECTED");
            xEventGroupClearBits(s_mqtt_evg, CONNECTED_BIT);
            ESP_LOGI(TAG, "mqtt_event_handler: MQTT_EVENT_DISCONNECTED");
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

__attribute__((unused)) uint8_t num_active_beacon()
{
    uint8_t num_act_beac = 0;
    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            num_act_beac++;
        }
    }
    return num_act_beac;
}

__attribute__((unused)) uint8_t first_active_beacon()
{
    uint8_t first_act_beac = UNKNOWN_BEACON;

    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            first_act_beac = i;
        }
    }

    return first_act_beac;
}

bool is_beacon_idx_active(uint16_t idx)
{
    return (s_active_beacon_mask & (1 << idx) );
}

void set_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask |= (1 << idx);
    persist_active_beacon_mask();
}

__attribute__((unused)) void clear_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask &= ~(1 << idx);
    persist_active_beacon_mask();
}

__attribute__((unused)) bool toggle_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask ^= (1 << idx);
    persist_active_beacon_mask();

    return s_active_beacon_mask & (1 << idx);
}

void persist_active_beacon_mask()
{
    esp_err_t err;
    UNUSED(err);

    if(CONFIG_ACTIVE_BLE_DEVICE_PERSISTANCE){
        err = save_blemqttproxy_param();
    }
}

void update_adv_data(uint16_t maj, uint16_t min, int8_t measured_power,
    float temp, float humidity, uint16_t battery, bool mqtt_send)
{
    uint8_t  idx = beacon_maj_min_to_idx(maj, min);

    ble_adv_data[idx].measured_power = measured_power;
    ble_adv_data[idx].temp           = temp;
    ble_adv_data[idx].humidity       = humidity;
    ble_adv_data[idx].battery        = battery;
    ble_adv_data[idx].last_seen      = esp_timer_get_time();

    if(mqtt_send){
        ESP_LOGD(TAG, "update_adv_data, update mqtt_last_send");
        ble_adv_data[idx].mqtt_last_send    = esp_timer_get_time();
    }

    s_display_status.current_beac = UNKNOWN_BEACON;
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void check_update_display(uint16_t maj, uint16_t min)
{
    uint8_t  idx = beacon_maj_min_to_idx(maj, min);

    if( (s_display_status.current_screen == BEACON_SCREEN) && (s_display_status.current_beac == idx) ){
        s_display_status.current_beac = UNKNOWN_BEACON; // invalidate beacon to force update
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
        return;
    } else if ( (s_display_status.current_screen == LASTSEEN_SCREEN) ){
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
        return;
    }
}

beacon_type_t esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len, uint8_t scan_rsp_len){
    beacon_type_t result = UNKNOWN_BEACON;

    ESP_LOG_BUFFER_HEXDUMP(TAG, adv_data, adv_data_len, ESP_LOG_DEBUG);

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if ( (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head_v3, sizeof(mybeacon_common_head_v3)))
            && (!memcmp((uint8_t*)(adv_data + sizeof(mybeacon_common_head_v3)),
                (uint8_t*)&mybeacon_common_vendor_v3, sizeof(mybeacon_common_vendor_v3.proximity_uuid)))){
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v3, true");
            result = BEACON_V3;
        } else {
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v3, false");
            result = UNKNOWN_BEACON;
        }
    } else if ((adv_data != NULL) && (adv_data_len == 0x17)){
        if ( (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head_v4, sizeof(mybeacon_common_head_v4))) ){
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v4, true");

            result = (scan_rsp_len ? BEACON_V4_SR : BEACON_V4);
        } else {
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v4, false");
            result = UNKNOWN_BEACON;
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

        beacon_type_t beacon_type = UNKNOWN_BEACON;
        uint16_t maj = 0, min = 0, battery = 0;
        int16_t x = 0, y = 0, z = 0;
        uint8_t idx = 0;
        float temp = 0, humidity = 0;

        bool is_beacon_active = true;
        bool mqtt_send_adv = false;

        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");
/*
            uint8_t *adv_name = NULL;
            uint8_t adv_name_len = 0;

            ESP_LOGI(TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
		    adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv, ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
		    ESP_LOGI(TAG, "searched Device Name Len %d", adv_name_len);
            esp_log_buffer_char(TAG, adv_name, adv_name_len);

            if (scan_result->scan_rst.adv_data_len > 0) {
                ESP_LOGI(GATTC_TAG, "adv data:");
                esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0) {
                ESP_LOGI(GATTC_TAG, "scan resp:");
                esp_log_buffer_hex(GATTC_TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
            }
*/
            beacon_type = esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);

            if( (beacon_type == BEACON_V3) || (beacon_type == BEACON_V4) || (beacon_type == BEACON_V4_SR) ){

                ESP_LOGD(TAG, "mybeacon found, type %d", beacon_type);

                switch(beacon_type){
                case BEACON_V3: {
                    esp_ble_mybeacon_v3_t *mybeacon_data = (esp_ble_mybeacon_v3_t*)(scan_result->scan_rst.ble_adv);

                    maj      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major);
                    min      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor);
                    idx      = beacon_maj_min_to_idx(maj, min);

                    if( (idx != UNKNOWN_BEACON) && (!is_beacon_idx_active(idx)) ){
                        if(scan_result->scan_rst.rssi > CONFIG_PROXIMITY_RSSI_THRESHOLD) {
                            ESP_LOGI(TAG, "Announcing new mybeacon (0x%04x%04x), idx %d, RSSI %d", maj, min, idx,
                                scan_result->scan_rst.rssi);
                            set_beacon_idx_active(idx);
                        } else {
                            ESP_LOGD(TAG, "mybeacon not active, not close enough (0x%04x%04x), idx %d, RSSI %d",
                                maj, min, idx, scan_result->scan_rst.rssi);
                            is_beacon_active = false;
                            break;
                        }
                    }

                    temp         = SHT3_GET_TEMPERATURE_VALUE(
                                        LSB_16(mybeacon_data->mybeacon_payload.temp),
                                        MSB_16(mybeacon_data->mybeacon_payload.temp) );
                    humidity    = SHT3_GET_HUMIDITY_VALUE(
                                        LSB_16(mybeacon_data->mybeacon_payload.humidity),
                                        MSB_16(mybeacon_data->mybeacon_payload.humidity) );
                    battery     = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery);
                    x           = (int16_t)(mybeacon_data->mybeacon_payload.x);
                    y           = (int16_t)(mybeacon_data->mybeacon_payload.y);
                    z           = (int16_t)(mybeacon_data->mybeacon_payload.z);

                    break;
                }
                case BEACON_V4:
                case BEACON_V4_SR:
                {
                    esp_ble_mybeacon_payload_t *mybeacon_payload = (esp_ble_mybeacon_payload_t *)(&scan_result->scan_rst.ble_adv[11]);

                    ESP_LOG_BUFFER_HEXDUMP(TAG, &scan_result->scan_rst.ble_adv[11], scan_result->scan_rst.adv_data_len-11, ESP_LOG_DEBUG);

                    maj         = (uint16_t) ( ((scan_result->scan_rst.ble_adv[7])<<8) + (scan_result->scan_rst.ble_adv[8]));
                    min         = (uint16_t) ( ((scan_result->scan_rst.ble_adv[9])<<8) + (scan_result->scan_rst.ble_adv[10]));
                    idx         = beacon_maj_min_to_idx(maj, min);
                    temp        = SHT3_GET_TEMPERATURE_VALUE(LSB_16(mybeacon_payload->temp), MSB_16(mybeacon_payload->temp) );
                    humidity    = SHT3_GET_HUMIDITY_VALUE(LSB_16(mybeacon_payload->humidity), MSB_16(mybeacon_payload->humidity) );
                    battery     = ENDIAN_CHANGE_U16(mybeacon_payload->battery);
                    x           = (int16_t)(mybeacon_payload->x);
                    y           = (int16_t)(mybeacon_payload->y);
                    z           = (int16_t)(mybeacon_payload->z);

                    break;
                }
                default:
                    ESP_LOGE(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: should not happen");
                    break;
                }

                if(!is_beacon_active){
                    break;
                }

                // ESP_LOGI(TAG, "free heap %d", esp_get_free_heap_size());


#if CONFIG_USE_MQTT==1
                uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_adv_data[idx].mqtt_last_send)/1000000;
                if( (mqtt_last_send_sec_gone > CONFIG_MQTT_MIN_TIME_INTERVAL_BETWEEN_MESSAGES)
                    || (ble_adv_data[idx].mqtt_last_send == 0)){
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
                        msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                        ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                        if(msg_id == -1){
                            mqtt_send_adv = false;
                        }
                    }
                    if( (humidity < CONFIG_HUMIDITY_LOW) || (humidity > CONFIG_HUMIDITY_HIGH) ){
                        ESP_LOGE(TAG, "humidity out of range, not send");
                    } else {
                        snprintf(buffer_topic, 128, CONFIG_MQTT_FORMAT, "beac", maj, min, "humidity");
                        snprintf(buffer_payload, 128, "%.2f", humidity);
                        msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
                        ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
                        if(msg_id == -1){
                            mqtt_send_adv = false;
                        }
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
                        if(msg_id == -1){
                            mqtt_send_adv = false;
                        }
                    }
                }
#endif // CONFIG_USE_MQTT
                ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d | mqtt send %c",
                    maj, min, scan_result->scan_rst.rssi, temp, humidity, x, y, z, battery, (mqtt_send_adv ? 'y':'n') );
                update_adv_data(maj, min, scan_result->scan_rst.rssi, temp, humidity, battery, mqtt_send_adv);
                check_update_display(maj, min);
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
    const esp_timer_create_args_t oneshot_timer_args = {
            .callback = &oneshot_timer_callback,
            .arg      = &oneshot_timer_usage,
            .name     = "oneshot"
    };

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            .name     = "periodic"
    };

    const esp_timer_create_args_t periodic_wdt_timer_args = {
            .callback = &periodic_wdt_timer_callback,
            .name     = "periodic_wdt"
    };

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_create(&periodic_wdt_timer_args, &periodic_wdt_timer));
}

#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
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

    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
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

    ESP_LOGI(TAG, "read_blemqttproxy_param: blemqttproxy_param.active_beacon_mask = %d", s_active_beacon_mask);

    return ret;
}

static esp_err_t save_blemqttproxy_param()
{
    esp_err_t ret;
    uint16_t mask = 0xFFFF;
    mask >>= 16 - CONFIG_BLE_DEVICE_COUNT_USE;

    blemqttproxy_param.active_beacon_mask = (blemqttproxy_param.active_beacon_mask & ~mask) | (s_active_beacon_mask & mask);

    ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    if(ret == ESP_OK){
        ESP_LOGV(TAG, "save_blemqttproxy_param: save param ok, beacon mask %u", blemqttproxy_param.active_beacon_mask);
    } else {
        ESP_LOGE(TAG, "save_blemqttproxy_param: save param failed, ret = %d, initialize and save to NVS", ret);
        blemqttproxy_param.active_beacon_mask = s_active_beacon_mask;
        ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    }

    s_active_beacon_mask = blemqttproxy_param.active_beacon_mask & mask;

    ESP_LOGV(TAG, "save_blemqttproxy_param: blemqttproxy_param.active_beacon_mask = %d", s_active_beacon_mask);

    return ret;
}

#if (CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1 || CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1)
static void wdt_task(void* pvParameters)
{
    EventBits_t uxBits;
    char buffer_topic[128];
    char buffer_payload[128];
    uint32_t uptime_sec;
    int msg_id = 0;

    periodic_wdt_timer_start();

    while (1) {
        uxBits = xEventGroupWaitBits(s_wdt_evg, UPDATE_ESP_RESTART | UPDATE_ESP_MQTT_RESTART, pdTRUE, pdFALSE, portMAX_DELAY);

        if(uxBits & UPDATE_ESP_MQTT_RESTART){
            ESP_LOGE(TAG, "ssd1306_update: reboot send mqtt flag is set -> send MQTT message");
            uptime_sec = esp_timer_get_time()/1000000;

            snprintf(buffer_topic, 128,  CONFIG_MQTT_FORMAT, "beac",
                ble_beacon_data[esp_restart_mqtt_beacon_to_take].major, ble_beacon_data[esp_restart_mqtt_beacon_to_take].minor, "reboot");
            snprintf(buffer_payload, 128, "%d", uptime_sec);
            msg_id = esp_mqtt_client_publish(s_client, buffer_topic, buffer_payload, 0, 1, 0);
            ESP_LOGD(TAG, "sent publish successful, msg_id=%d", msg_id);
            fflush(stdout);
        }

        if(uxBits & UPDATE_ESP_RESTART){
            ESP_LOGE(TAG, "ssd1306_update: reboot flag is set -> esp_restart() -> REBOOT");
            fflush(stdout);
            esp_restart();
        }
    }
    vTaskDelete(NULL);
}
#endif

void app_main()
{
    // NVS initialization and beacon mask retrieval
    initialize_nvs();
    ESP_ERROR_CHECK(read_blemqttproxy_param());

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    s_values_evg = xEventGroupCreate();
    s_wifi_evg   = xEventGroupCreate();
    s_mqtt_evg   = xEventGroupCreate();
    s_wdt_evg    = xEventGroupCreate();

    create_timer();

#if CONFIG_DISABLE_BUTTON_HEADLESS==0
    button_handle_t btn_handle = iot_button_create(BUTTON_IO_NUM, BUTTON_ACTIVE_LEVEL);
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_PUSH, button_push_cb, "PUSH");
    iot_button_set_evt_cb(btn_handle, BUTTON_CB_RELEASE, button_release_cb, "RELEASE");
#endif // CONFIG_DISABLE_BUTTON_HEADLESS

#ifdef CONFIG_DISPLAY_SSD1306
    xTaskCreate(&ssd1306_task, "ssd1306_task", 2048 * 2, NULL, 5, NULL);
    vTaskDelay(50 / portTICK_PERIOD_MS);
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);

    oneshot_timer_usage = SPLASH_SCREEN;
    ESP_LOGD(TAG, "app_main, start oneshot timer, %d", SPLASH_SCREEN_TIMER_DURATION);
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, SPLASH_SCREEN_TIMER_DURATION));
#endif // CONFIG_DISPLAY_SSD1306

    wifi_init();
    mqtt_init();

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));

    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));


#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
    init_owb_tempsensor();
    s_display_status.num_localtemp_pages = (!s_owb_num_devices ? 1 : s_owb_num_devices);
    xTaskCreate(&localsensor_task, "localsensor_task", 2048 * 2, NULL, 5, NULL);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);

    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&ble_scan_params));

#if (CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1 || CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1)
    xTaskCreate(&wdt_task, "wdt_task", 2048 * 2, NULL, 5, NULL);
#endif
}
