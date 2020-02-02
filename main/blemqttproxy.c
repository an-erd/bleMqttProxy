#include <sdkconfig.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"
#include <sys/param.h>

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
#include "esp_gatt_common_api.h"
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
#include <esp_http_server.h>

#include "helperfunctions.h"
#include "display.h"
#include "ble.h"
#include "beacon.h"
#include "localsensor.h"
#include "ble_mqtt.h"
#include "timer.h"
#include "web_file_server.h"

static const char* TAG = "BLEMQTTPROXY";

// IOT param
#define PARAM_NAMESPACE "blemqttproxy"
#define PARAM_KEY       "activebeac"

typedef struct {
    uint16_t active_beacon_mask;
} param_t;
param_t blemqttproxy_param = { 0 };

esp_err_t read_blemqttproxy_param();
esp_err_t save_blemqttproxy_param();

// BLE
const uint8_t const REMOTE_SERVICE_UUID[] = {0x3C, 0xB7, 0xA2, 0x4B, 0x0C, 0x32, 0xF2, 0x9F, 0x4F, 0x4C, 0xF5, 0x37, 0x00, 0x14, 0x2F, 0x61};
#define REMOTE_NOTIFY_CHAR_UUID         0x1401
#define REMOTE_INDICATION_CHAR_UUID     0x2A52
#define REMOTE_NOTIFY_HANDLE            0x26
#define REMOTE_INDICATE_HANDLE          0x2B

#define PROFILE_NUM      2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define INVALID_HANDLE   0

static const char remote_device_name[] = "Bx0706";
static bool gattc_connect    = false;
static bool get_server = false;

static bool device_notify_1401          = false;
static bool device_indicate_2A52        = false;
static esp_gattc_char_elem_t *char_elem_result              = NULL;
static esp_gattc_descr_elem_t *descr_elem_result            = NULL;

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

static esp_bt_uuid_t remote_filter_service_uuid;

static esp_bt_uuid_t remote_filter_char_uuid_1401 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_bt_uuid_t remote_filter_char_uuid_2A52 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_INDICATION_CHAR_UUID,},
};

static esp_bt_uuid_t indication_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle_1401;
    uint16_t char_handle_2A52;
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

typedef struct
{
    uint16_t        sequence_number;                            /**< Sequence number */
    uint32_t        time_stamp;                                 /**< Time stamp */
    uint16_t        temperature;                                /**< Sensor temperature value */
    uint16_t        humidity;                                   /**< Sensor humidity value */
} __attribute__ ((packed)) ble_os_meas_t;

ble_os_meas_t buffer_download[1250];
uint16_t      buffer_download_count = 0;

// Wifi
EventGroupHandle_t s_wifi_evg;
const int CONNECTED_BIT = BIT0;
uint16_t wifi_connections_connect = 0;
uint16_t wifi_connections_disconnect = 0;

// Watchdog timer / WDT
static esp_timer_handle_t periodic_wdt_timer;
static void periodic_wdt_timer_callback(void* arg);
static void periodic_wdt_timer_start();
#define UPDATE_ESP_RESTART          (BIT0)
#define UPDATE_ESP_MQTT_RESTART     (BIT1)
static uint8_t esp_restart_mqtt_beacon_to_take = UNKNOWN_BEACON;
static EventGroupHandle_t s_wdt_evg;
#define WDT_TIMER_DURATION              (CONFIG_WDT_OWN_INTERVAL * 1000000)


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


void clear_stats_values()
{
    wifi_connections_connect = 0;
    wifi_connections_disconnect = 0;
    mqtt_packets_send = 0;
    mqtt_packets_fail = 0;
}

#if CONFIG_DISABLE_BUTTON_HEADLESS==0
void handle_long_button_push()
{
    switch(s_display_status.current_screen){
        case BEACON_SCREEN:
            toggle_beacon_idx_active(s_display_status.beac_to_show);
            break;
        case LASTSEEN_SCREEN:
            break;
        case LOCALTEMP_SCREEN:
            break;
        case APPVERSION_SCREEN:
            break;
        case STATS_SCREEN:
            clear_stats_values();
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

    if(!s_display_status.display_on){
        ESP_LOGD(TAG, "button_release_cb: turn display on again");

        set_run_idle_timer(true);
        if( (s_display_status.current_screen == LASTSEEN_SCREEN)
            || (s_display_status.current_screen ==  APPVERSION_SCREEN)
            || (s_display_status.current_screen ==  STATS_SCREEN) ){
            set_run_periodic_timer(true);
        }
        turn_display_off = false;
        xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
        return;
    }

    set_run_idle_timer_touch(true);

    ESP_LOGD(TAG, "button_release_cb: s_display_status.current_screen %d screen_to_show %d >",
        s_display_status.current_screen, s_display_status.screen_to_show);

    if(esp_timer_get_time() < time_button_long_press){
        set_next_display_show();
    } else {
        handle_long_button_push();
    }

    switch(s_display_status.screen_to_show){
        case BEACON_SCREEN:
            set_run_periodic_timer(false);
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case LASTSEEN_SCREEN:
            set_run_periodic_timer(true);
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case LOCALTEMP_SCREEN:
            set_run_periodic_timer(false);
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case APPVERSION_SCREEN:
            set_run_periodic_timer(true);
            xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
            break;
        case STATS_SCREEN:
            set_run_periodic_timer(true);
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


__attribute__((unused)) void periodic_wdt_timer_start(){
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_wdt_timer, WDT_TIMER_DURATION));
}

void periodic_wdt_timer_callback(void* arg)
{
    EventBits_t uxSet, uxReturn;

    // ESP_LOGD(TAG, "periodic_wdt_timer_callback(): >>");

    uint8_t beacon_to_take = UNKNOWN_BEACON;
    uint16_t lowest_last_seen_sec = CONFIG_WDT_LAST_SEEN_THRESHOLD;
    uint16_t temp_last_seen_sec;

    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            temp_last_seen_sec = (esp_timer_get_time() - ble_adv_data[i].last_seen)/1000000;
            if(temp_last_seen_sec < lowest_last_seen_sec){
                beacon_to_take = i;
                lowest_last_seen_sec = temp_last_seen_sec;
            }
        }
    }

    // ESP_LOGD(TAG, "periodic_wdt_timer_callback: lowest beac found %d", beacon_to_take);
    if(beacon_to_take == UNKNOWN_BEACON){
        // ESP_LOGD(TAG, "periodic_wdt_timer_callback: no active beac <<");
        return;
    }

    // ESP_LOGD(TAG, "periodic_wdt_timer_callback(): last_seen_sec_gone = %d", lowest_last_seen_sec);
    if(lowest_last_seen_sec >= CONFIG_WDT_LAST_SEEN_THRESHOLD){

        uxReturn = xEventGroupWaitBits(s_mqtt_evg, CONNECTED_BIT, false, true, 0);
        bool mqtt_connected = uxReturn & CONNECTED_BIT;

        uxReturn = xEventGroupWaitBits(s_wifi_evg, CONNECTED_BIT, false, true, 0);
        bool wifi_connected = uxReturn & CONNECTED_BIT;

        ESP_LOGE(TAG, "periodic_wdt_timer_callback: last seen > threshold: %d sec, WIFI: %s, MQTT (enabled/onnected): %s/%s", lowest_last_seen_sec,
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

static esp_err_t wifi_event_handler(void *ctx, system_event_t *event)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;

    switch (event->event_id) {
        case SYSTEM_EVENT_STA_START:
            esp_wifi_connect();
            break;
        case SYSTEM_EVENT_STA_GOT_IP:
            ESP_LOGI(TAG, "wifi_event_handler: SYSTEM_EVENT_STA_GOT_IP, IP: '%s'", ip4addr_ntoa(&event->event_info.got_ip.ip_info.ip));
            wifi_connections_connect++;

            if (*server == NULL) {
                *server = start_webserver();
            }

            xEventGroupSetBits(s_wifi_evg, CONNECTED_BIT);
            break;
        case SYSTEM_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "wifi_event_handler: SYSTEM_EVENT_STA_DISCONNECTED");
            wifi_connections_disconnect++;

            if (*server) {
                stop_webserver(*server);
                *server = NULL;
            }

            esp_wifi_connect();
            xEventGroupClearBits(s_wifi_evg, CONNECTED_BIT);
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void wifi_init(void *arg)
{
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(wifi_event_handler, arg));
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

static const char *esp_key_type_to_str(esp_ble_key_type_t key_type)
{
   const char *key_str = NULL;
   switch(key_type) {
    case ESP_LE_KEY_NONE:
        key_str = "ESP_LE_KEY_NONE";
        break;
    case ESP_LE_KEY_PENC:
        key_str = "ESP_LE_KEY_PENC";
        break;
    case ESP_LE_KEY_PID:
        key_str = "ESP_LE_KEY_PID";
        break;
    case ESP_LE_KEY_PCSRK:
        key_str = "ESP_LE_KEY_PCSRK";
        break;
    case ESP_LE_KEY_PLK:
        key_str = "ESP_LE_KEY_PLK";
        break;
    case ESP_LE_KEY_LLK:
        key_str = "ESP_LE_KEY_LLK";
        break;
    case ESP_LE_KEY_LENC:
        key_str = "ESP_LE_KEY_LENC";
        break;
    case ESP_LE_KEY_LID:
        key_str = "ESP_LE_KEY_LID";
        break;
    case ESP_LE_KEY_LCSRK:
        key_str = "ESP_LE_KEY_LCSRK";
        break;
    default:
        key_str = "INVALID BLE KEY TYPE";
        break;

    }
     return key_str;
}

static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req)
{
   char *auth_str = NULL;
   switch(auth_req) {
    case ESP_LE_AUTH_NO_BOND:
        auth_str = "ESP_LE_AUTH_NO_BOND";
        break;
    case ESP_LE_AUTH_BOND:
        auth_str = "ESP_LE_AUTH_BOND";
        break;
    case ESP_LE_AUTH_REQ_MITM:
        auth_str = "ESP_LE_AUTH_REQ_MITM";
        break;
    case ESP_LE_AUTH_REQ_BOND_MITM:
        auth_str = "ESP_LE_AUTH_REQ_BOND_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_ONLY:
        auth_str = "ESP_LE_AUTH_REQ_SC_ONLY";
        break;
    case ESP_LE_AUTH_REQ_SC_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_BOND";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM_BOND";
        break;
    default:
        auth_str = "INVALID BLE AUTH REQ";
        break;
   }

   return auth_str;
}

static void show_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    ESP_LOGI(TAG, "Bonded devices number : %d\n", dev_num);

    ESP_LOGI(TAG, "Bonded devices list : %d\n", dev_num);
    for (int i = 0; i < dev_num; i++) {
        esp_log_buffer_hex(TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
    }

    free(dev_list);
}

static void __attribute__((unused)) remove_all_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }

    free(dev_list);
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_REG_EVT");
        esp_ble_gap_config_local_privacy(true);
        break;
    case ESP_GATTC_CONNECT_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT");
        ESP_LOGI(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "REMOTE BDA:");
        esp_log_buffer_hex(TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_OPEN_EVT");
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(TAG, "open success");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_CFG_MTU_EVT");
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGD(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        ESP_LOGD(TAG, "wait now until ESP_GAP_BLE_AUTH_CMPL_EVT");
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_RES_EVT");
        ESP_LOGD(TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGD(TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16){
            ESP_LOGI(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_32){
            ESP_LOGI(TAG, "UUID32: %x", p_data->search_res.srvc_id.uuid.uuid.uuid32);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128){
            esp_gatt_id_t *srvc_id = &p_data->search_res.srvc_id;
            ESP_LOGI(TAG, "UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",       srvc_id->uuid.uuid.uuid128[0],
                     srvc_id->uuid.uuid.uuid128[1], srvc_id->uuid.uuid.uuid128[2], srvc_id->uuid.uuid.uuid128[3],
                     srvc_id->uuid.uuid.uuid128[4], srvc_id->uuid.uuid.uuid128[5], srvc_id->uuid.uuid.uuid128[6],
                     srvc_id->uuid.uuid.uuid128[7], srvc_id->uuid.uuid.uuid128[8], srvc_id->uuid.uuid.uuid128[9],
                     srvc_id->uuid.uuid.uuid128[10], srvc_id->uuid.uuid.uuid128[11], srvc_id->uuid.uuid.uuid128[12],
                     srvc_id->uuid.uuid.uuid128[13], srvc_id->uuid.uuid.uuid128[14], srvc_id->uuid.uuid.uuid128[15]);
            ESP_LOGI(TAG, "service found, start_handle 0x%X end_handle 0x%X", p_data->search_res.start_handle, p_data->search_res.end_handle);
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGI(TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGI(TAG, "Get service information from flash");
        } else {
            ESP_LOGI(TAG, "unknown service source");
        }
        ESP_LOGI(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        ESP_LOGI(TAG, "get_server %d", get_server);
        if (get_server){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                     gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            ESP_LOGI(TAG, "count %d", count);
            if (count > 0){
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(TAG, "gattc no mem");
                }else{
                    // 1401
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                             remote_filter_char_uuid_1401,
                                                             char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_tab[PROFILE_A_APP_ID].char_handle_1401 = char_elem_result[0].char_handle;
                        ESP_LOGI(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with notify", char_elem_result[0].char_handle);
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                    }

                    // 2A52
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                             gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                             remote_filter_char_uuid_2A52,
                                                             char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_INDICATE)){
                        gl_profile_tab[PROFILE_A_APP_ID].char_handle_2A52 = char_elem_result[0].char_handle;
                        ESP_LOGI(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with indicate", char_elem_result[0].char_handle);
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result);
            }else{
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count          = 0;
            uint16_t notify_en      = 1;
            uint16_t indicate_en    = 2;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].char_handle_1401,
                                                                         &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            ESP_LOGI(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT count = %d", count);
            if (count > 0){
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result){
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }

                    ESP_LOGI(TAG, "esp_ble_gattc_write_char_descr (notify) 0x%X", descr_elem_result[0].handle);

                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        if (descr_elem_result[0].handle == 0x26){
                            ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(notify_en),
                                                                        (uint8_t *)&notify_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                        } else if (descr_elem_result[0].handle == 0x2B){
                            ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(indicate_en),
                                                                        (uint8_t *)&indicate_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                        }
                    }

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else{
                ESP_LOGE(TAG, "decsr not found");
            }

        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT");

        static int64_t time_measure = 0;
        uint8_t len = 0;

        if (p_data->notify.is_notify){
            if(!buffer_download_count){
                ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, first");
                time_measure = esp_timer_get_time();
            }

            // ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive notify value:");
            buffer_download[buffer_download_count].sequence_number  = uint16_decode(&p_data->notify.value[len]); len += 2; // uint16_t
            buffer_download[buffer_download_count].time_stamp       = uint32_decode(&p_data->notify.value[len]); len += 4; // time_t
            buffer_download[buffer_download_count].temperature      = uint16_decode(&p_data->notify.value[len]); len += 2; // uint16_t
            buffer_download[buffer_download_count].humidity         = uint16_decode(&p_data->notify.value[len]); len += 2; // uint16_t
            // esp_log_buffer_hex(TAG, &buffer_download[buffer_download_count], p_data->notify.value_len);
            buffer_download_count++;
        }else{
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, done");
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");

            ESP_LOGI(TAG, "buffer_download_count %d, time difference %d", buffer_download_count, (uint16_t) (esp_timer_get_time() - time_measure));
        }
        // esp_log_buffer_hex(TAG, p_data->notify.value, p_data->notify.value_len);

        if (!p_data->notify.is_notify){
            ESP_LOGI(TAG, "now disconnecting");
            esp_gatt_status_t ret_status = esp_ble_gattc_close(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, 0);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_close, error status %d", ret_status);
            }
        }
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "write descr success handle 0x%X", p_data->write.handle);
        if(p_data->write.handle == REMOTE_NOTIFY_HANDLE){
            device_notify_1401 = true;
        } else if(p_data->write.handle == REMOTE_INDICATE_HANDLE){
            device_indicate_2A52 = true;
        }

        if (! (device_notify_1401 && device_indicate_2A52) )
            break;

        uint8_t write_char_data[2];
        write_char_data[0] = 0x01;
        write_char_data[1] = 0x01;      // TODO send last, send all, ...

        ESP_LOGI(TAG, "esp_ble_gattc_write_char 0x%X", gl_profile_tab[PROFILE_A_APP_ID].char_handle_2A52);
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_A_APP_ID].char_handle_2A52,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NO_MITM);       // ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        esp_log_buffer_hex(TAG, bda, sizeof(esp_bd_addr_t));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write char failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT");
        gattc_connect = false;
        get_server = false;
        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);

        uint32_t duration = 0;  // scan permanently
        esp_ble_gap_start_scanning(duration);
        break;
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT");
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "config local privacy failed, error code =%x", param->local_privacy_cmpl.status);
            break;
        }
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
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
        ESP_LOGI(TAG, "scan start success");
        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        /* Call the following function to input the passkey which is displayed on the remote device */
        //esp_ble_passkey_reply(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, true, 0x00);
        ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
        ESP_LOGI(TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
        esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
    }
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
        ESP_LOGI(TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
        break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
        ESP_LOGI(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should send the security response with negative(false) accept value*/
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT");
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        ESP_LOGI(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
        ESP_LOGI(TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT");
        ///show the passkey number to the user to input it in the peer device.
        ESP_LOGI(TAG, "The passkey Notify number:%06d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_KEY_EVT:
        ESP_LOGI(TAG, "ESP_GAP_BLE_KEY_EVT");
        //shows the ble key info share with peer device to the user.
        ESP_LOGI(TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        ESP_LOGI(TAG, "ESP_GAP_BLE_AUTH_CMPL_EVT");
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        } else {
            ESP_LOGI(TAG, "auth mode = %s",esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
        }

        // TODO just a try
        ESP_LOGI(TAG, "esp_ble_gattc_search_service");
        esp_ble_gattc_search_service(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id, &remote_filter_service_uuid);

        break;
    }
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

            esp_log_buffer_hex(TAG, scan_result->scan_rst.bda, 6);
            ESP_LOGI(TAG, "searched Adv Data Len %d, Scan Response Len %d", scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);
            adv_name = esp_ble_resolve_adv_data(scan_result->scan_rst.ble_adv,
                                                ESP_BLE_AD_TYPE_NAME_CMPL, &adv_name_len);
            ESP_LOGI(TAG, "searched Device Name Len %d", adv_name_len);
            esp_log_buffer_char(TAG, adv_name, adv_name_len);

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0) {
                ESP_LOGI(TAG, "adv data:");
                esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0) {
                ESP_LOGI(TAG, "scan resp:");
                esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
            }
#endif

            beacon_type = esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);

            if( (beacon_type == BEACON_V3) || (beacon_type == BEACON_V4) || (beacon_type == BEACON_V4_SR) ){

                ESP_LOGD(TAG, "mybeacon found, type %d", beacon_type);
                switch(beacon_type){

                case BEACON_V3: {
                    decode_mybeacon_packet_v3((esp_ble_mybeacon_v3_t*)(scan_result->scan_rst.ble_adv), &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active);
                    break;
                }

                case BEACON_V4:
                case BEACON_V4_SR:
                {
                    esp_ble_mybeacon_payload_t *mybeacon_payload = (esp_ble_mybeacon_payload_t *)(&scan_result->scan_rst.ble_adv[11]);

                    decode_mybeacon_packet_v4(mybeacon_payload, scan_result->scan_rst.ble_adv, &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active);
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

                mqtt_send_adv = send_to_mqtt(idx, maj, min, temp, humidity, battery, scan_result->scan_rst.rssi);

               ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d | mqtt send %c",
                    maj, min, scan_result->scan_rst.rssi, temp, humidity, x, y, z, battery, (mqtt_send_adv ? 'y':'n') );

                update_adv_data(maj, min, scan_result->scan_rst.rssi, temp, humidity, battery, mqtt_send_adv);
                check_update_display(maj, min);
// TODO Connect to device
                if (adv_name != NULL) {
                    if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0) {
                        ESP_LOGI(TAG, "searched device %s\n", remote_device_name);
                        if (gattc_connect == false) {
                            gattc_connect = true;
                            ESP_LOGI(TAG, "connect to the remote device.");
                            esp_ble_gap_stop_scanning();
                            // show_bonded_devices();
                            // remove_all_bonded_devices();
                            esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                        }
                    }
                }
            } else {
                ESP_LOGD(TAG, "mybeacon not found");
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
            break;
        default:
            ESP_LOGE(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT - default");
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Stop scan successfully");
        }
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGI(TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        ESP_LOGE(TAG, "esp_gap_cb - default (%u)", event);
        break;
    }
}

static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        ESP_LOGD(TAG, "esp_gattc_cb ESP_GATTC_REG_EVT");
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                   param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_tab[idx].gattc_if) {
                if (gl_profile_tab[idx].gattc_cb) {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
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

esp_err_t read_blemqttproxy_param()
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

esp_err_t save_blemqttproxy_param()
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

static void wifi_mqtt_task(void* pvParameters)
{
    static httpd_handle_t server = NULL;
    // EventBits_t uxBits;

    wifi_init(&server);
    mqtt_init();

    // while (1) {
    //     uxBits = xEventGroupWaitBits(s_wdt_evg, ... , pdTRUE, pdFALSE, portMAX_DELAY);
    // }
    vTaskDelete(NULL);
}

void app_main()
{

    // adjust log level
    esp_log_level_set("wifi", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);
    esp_log_level_set("efuse", ESP_LOG_WARN);
    esp_log_level_set("tcpip_adapter", ESP_LOG_WARN);
    esp_log_level_set("nvs", ESP_LOG_WARN);
    esp_log_level_set("memory_layout", ESP_LOG_WARN);
    esp_log_level_set("heap_init", ESP_LOG_WARN);
    esp_log_level_set("intr_alloc", ESP_LOG_WARN);
    esp_log_level_set("BT_BTM", ESP_LOG_WARN);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_INFO);
    esp_log_level_set("httpd_uri", ESP_LOG_INFO);
    esp_log_level_set("BTDM_INIT", ESP_LOG_INFO);
    esp_log_level_set("timer", ESP_LOG_INFO);



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

    oneshot_timer_usage = TIMER_SPLASH_SCREEN;
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);

    ESP_LOGI(TAG, "app_main, start oneshot timer, %d", SPLASH_SCREEN_TIMER_DURATION);
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, SPLASH_SCREEN_TIMER_DURATION));
#endif // CONFIG_DISPLAY_SSD1306

    xTaskCreate(&wifi_mqtt_task, "wifi_mqtt_task", 2048 * 2, NULL, 5, NULL);

    remote_filter_service_uuid.len = ESP_UUID_LEN_128;
    memcpy(remote_filter_service_uuid.uuid.uuid128, REMOTE_SERVICE_UUID, 16);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(PROFILE_A_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));

    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));

#if CONFIG_LOCAL_SENSORS_TEMPERATURE==1
    init_owb_tempsensor();
    s_display_status.num_localtemp_pages = (!s_owb_num_devices ? 1 : s_owb_num_devices);
    xTaskCreate(&localsensor_task, "localsensor_task", 2048 * 2, NULL, 5, NULL);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE

#if (CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1 || CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1)
    xTaskCreate(&wdt_task, "wdt_task", 2048 * 2, NULL, 5, NULL);
#endif
}
