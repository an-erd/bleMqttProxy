#include <sdkconfig.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs_flash.h"
#include <sys/param.h>

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_assert.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "errno.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "esp_netif.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include <inttypes.h>
#include <esp_http_server.h>
#include "esp_http_client.h"

/* Littlevgl specific */
#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"

#include "mqtt_client.h"
#include "iot_button.h"
#include "iot_param.h"
#include "helperfunctions.h"
#include "display.h"
#include "ble.h"
#include "beacon.h"
#include "ble_mqtt.h"
#include "timer.h"
#include "web_file_server.h"
#include "offlinebuffer.h"
#include "ota.h"

static const char* TAG = "BLEMQTTPROXY";
static const char* GATTS_TAG = "GATTS";

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
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
static void show_bonded_devices(void);

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

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile_s_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_S_ID] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

// Wifi
EventGroupHandle_t wifi_evg;
#define  WIFI_CONNECTED_BIT         (BIT0)
uint16_t wifi_connections_count_connect = 0;
uint16_t wifi_connections_count_disconnect = 0;
tcpip_adapter_ip_info_t ipinfo;
uint8_t mac[6];

// WiFi AP
uint16_t wifi_ap_connections = 0;


// Watchdog timer / WDT
static esp_timer_handle_t periodic_wdt_timer;
static void periodic_wdt_timer_callback(void* arg);
static void periodic_wdt_timer_start();
#define UPDATE_ESP_RESTART          (BIT0)
#define UPDATE_ESP_MQTT_RESTART     (BIT1)
static uint8_t esp_restart_mqtt_beacon_to_take = UNKNOWN_BEACON;
static EventGroupHandle_t s_wdt_evg;
#define WDT_TIMER_DURATION          (CONFIG_WDT_OWN_INTERVAL * 1000000)

// Button
#define MAX_BUTTON_COUNT 3
#define BUTTON_ACTIVE_LEVEL 0
static int8_t btn[MAX_BUTTON_COUNT][2] = { {0, CONFIG_BUTTON_1_PIN}, {1, CONFIG_BUTTON_2_PIN}, {2, CONFIG_BUTTON_3_PIN} };
static button_handle_t btn_handle[MAX_BUTTON_COUNT];
static int64_t time_button_long_press[MAX_BUTTON_COUNT] = { 0 };

// TODO put to different position
void clear_stats_values()
{
    wifi_connections_count_connect = 0;
    wifi_connections_count_disconnect = 0;
    wifi_ap_connections = 0;
    mqtt_packets_send = 0;
    mqtt_packets_fail = 0;
}

void button_push_cb(void* arg)
{
    uint8_t btn = *((uint8_t*) arg);
    UNUSED(btn);

    if(!display_status.button_enabled){
        ESP_LOGD(TAG, "button_push_cb: button not enabled");
        return;
    }

    time_button_long_press[btn] = esp_timer_get_time() + CONFIG_LONG_PRESS_TIME * 1000;

    ESP_LOGD(TAG, "button_push_cb");
}

void handle_long_button_push()
{
    switch(display_status.current_screen){
        case BEACON_SCREEN:
            toggle_beacon_idx_active(display_status.beac_to_show);
            break;
        case LASTSEEN_SCREEN:
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

void turn_display_on()
{
    set_run_idle_timer(true);
    if( (display_status.current_screen == LASTSEEN_SCREEN)
        || (display_status.current_screen == APPVERSION_SCREEN)
        || (display_status.current_screen == STATS_SCREEN) ){
        set_run_periodic_timer(true);
    }
    turn_display_off = false;
}

void handle_button_display_message()
{
    oneshot_display_message_timer_touch();
    toggle_beacon_idx_active(display_message_content.beac);
    snprintf(display_message_content.comment, 128, "Activated: %c", (is_beacon_idx_active(display_message_content.beac)? 'y':'n'));
    display_message_content.need_refresh = true;
}

bool is_button_long_press(uint8_t btn)
{
    return esp_timer_get_time() >= time_button_long_press[btn];
}

void handle_set_next_display_show()
{
    switch(display_status.screen_to_show){
        case BEACON_SCREEN:
            set_run_periodic_timer(false);
            break;
        case LASTSEEN_SCREEN:
            set_run_periodic_timer(true);
            break;
        case APPVERSION_SCREEN:
            set_run_periodic_timer(true);
            break;
        case STATS_SCREEN:
            set_run_periodic_timer(true);
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }

}

void handle_m5stack_button(uint8_t btn, bool long_press)
{
    if((btn == 2) && !long_press){   // right button to switch to next screen
        set_next_display_show();
        return;
    }

    switch(display_status.current_screen){
        case BEACON_SCREEN:
            if(btn==0){
                toggle_beacon_idx_active(display_status.beac_to_show);
            } else if(btn==1){
                clear_beacon_idx_values(display_status.beac_to_show);
            }
            break;
        case LASTSEEN_SCREEN:
            break;
        case APPVERSION_SCREEN:
            break;
        case STATS_SCREEN:
            if(btn==1){
                clear_stats_values();
            }
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }
}

void button_release_cb(void* arg)
{
    uint8_t btn = *((uint8_t*) arg);
    bool is_long_press;

    ESP_LOGD(TAG, "Button pressed: %d", btn);

    if(!display_status.button_enabled){
        ESP_LOGI(TAG, "button_release_cb: button not enabled");
        return;
    }

    if(!display_status.display_on){
        ESP_LOGI(TAG, "button_release_cb: turn display on again");
        turn_display_on();
        return;
    }

    if(display_status.display_message_is_shown){
        handle_button_display_message();
        return;
    }
    set_run_idle_timer_touch(true);

    ESP_LOGD(TAG, "button_release_cb: display_status.current_screen %d screen_to_show %d >",
        display_status.current_screen, display_status.screen_to_show);

    is_long_press = is_button_long_press(btn);

#if defined CONFIG_DEVICE_WEMOS || defined CONFIG_DEVICE_M5STICK
    if(!is_long_press){
        set_next_display_show();
    } else {
        handle_long_button_push(btn);
    }
    handle_set_next_display_show();
#elif defined CONFIG_DEVICE_M5STICKC
    switch(btn){
        case 0:
            if(!is_long_press){
                set_next_display_show();
            } else {
                handle_long_button_push(btn);
            }
            break;
        case 1:
            handle_long_button_push(btn);   // TODO
            break;
        default:
            ESP_LOGE(TAG, "button_release_cb: unhandled switch case");
    }
    handle_set_next_display_show();
#elif defined CONFIG_DEVICE_M5STACK
    handle_m5stack_button(btn, is_long_press);
#endif

    ESP_LOGD(TAG, "button_release_cb: display_status.current_screen %d screen_to_show %d <",
        display_status.current_screen, display_status.screen_to_show);
}

__attribute__((unused)) void periodic_wdt_timer_start(){
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_wdt_timer, WDT_TIMER_DURATION));
}

static __attribute__((unused)) void send_mqtt_uptime_heap_last_seen(uint8_t num_act_beacon, uint16_t lowest_last_seen_sec, uint16_t lowest_last_send_sec){
    EventBits_t uxReturn;
    int msg_id = 0; UNUSED(msg_id);
    char buffer[32], buffer_topic[32], buffer_payload[32];
    bool wifi_connected, mqtt_connected;
    uint32_t uptime_sec;
    lv_mem_monitor_t mem_mon;

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
        ESP_LOGD(TAG, "MQTT %s -> uptime %s", buffer_topic, buffer_payload);
        msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

        snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "free_heap");
        snprintf_nowarn(buffer_payload, 32, "%d", esp_get_free_heap_size());
        ESP_LOGD(TAG, "MQTT %s -> free_heap %s", buffer_topic, buffer_payload);
        msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

        // LV Memory Monitor
        lv_mem_monitor(&mem_mon);
        ESP_LOGD(TAG, "lv_mem_monitor: Memory %d %%, Total %d bytes, used %d bytes, free %d bytes, frag %d %%",
            (int) mem_mon.used_pct, (int) mem_mon.total_size,
            (int) mem_mon.total_size - mem_mon.free_size, mem_mon.free_size, mem_mon.frag_pct);

        if(num_act_beacon > 0){
            snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "last_seen");
            snprintf_nowarn(buffer_payload, 32, "%d", lowest_last_seen_sec);
            ESP_LOGD(TAG, "MQTT %s -> lowest_last_seen_sec %s", buffer_topic, buffer_payload);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);

            snprintf_nowarn(buffer_topic, 32,  CONFIG_WDT_MQTT_FORMAT, buffer, "last_send");
            snprintf_nowarn(buffer_payload, 32, "%d", lowest_last_send_sec);
            ESP_LOGD(TAG, "MQTT %s -> lowest_last_send_sec %s", buffer_topic, buffer_payload);
            msg_id = mqtt_client_publish(mqtt_client, buffer_topic, buffer_payload, 0, 1, 0);
        }
    }
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
            xEventGroupSetBits(s_wdt_evg, uxSet);
        }
    }
}

static void event_handler(void* ctx, esp_event_base_t event_base,  int32_t event_id, void* event_data)
{
    httpd_handle_t *server = (httpd_handle_t *) ctx;
    EventBits_t uxReturn;

    if (event_base == WIFI_EVENT){
        switch(event_id){
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "event_handler: WIFI_EVENT_STA_START");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "event_handler: WIFI_EVENT_STA_CONNECTED");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "event_handler: WIFI_EVENT_STA_DISCONNECTED");
                wifi_connections_count_disconnect++;
                ipinfo.ip.addr = ((u32_t)0x00000000UL); // set ip addr to 0.0.0.0
                esp_wifi_connect();
                xEventGroupClearBits(wifi_evg, WIFI_CONNECTED_BIT);
                break;
            case WIFI_EVENT_AP_STACONNECTED: {
                wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
                ESP_LOGI(TAG, "event_handler: WIFI_EVENT_AP_STACONNECTED, station "MACSTR" join, AID=%d", MAC2STR(event->mac), event->aid);
                wifi_ap_connections++;
                }
                break;
            case WIFI_EVENT_AP_STADISCONNECTED: {
                wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
                ESP_LOGI(TAG, "event_handler: WIFI_EVENT_AP_STADISCONNECTED, station "MACSTR" leave, AID=%d", MAC2STR(event->mac), event->aid);
                wifi_ap_connections--;
                }
                break;
            default:
                break;
        }
    } else if (event_base == IP_EVENT){
        ESP_LOGI(TAG, "event_handler: IP_EVENT: %d", event_id);

        switch(event_id){
            case IP_EVENT_STA_GOT_IP: {
                tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipinfo);
                ESP_LOGI(TAG, "event_handler: IP_EVENT_STA_GOT_IP, IP: " IPSTR, IP2STR(&ipinfo.ip));
                wifi_connections_count_connect++;
                xEventGroupSetBits(wifi_evg, WIFI_CONNECTED_BIT);
                }
                break;
            default:
                break;
        }
    }

    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    if(wifi_connected || wifi_ap_connections){
        if (*server == NULL) {
            *server = start_webserver();
        }
    } else {
        if (*server) {
            stop_webserver(*server);
            *server = NULL;
        }
    }
}

static void wifi_init(void *arg)
{
    uint8_t mac[6];
    char buffer[32];
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, arg));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, arg));
    wifi_config_t wifi_config_sta, wifi_config_ap;
    memset(&wifi_config_sta, 0, sizeof(wifi_config_sta));
    memcpy((char *)wifi_config_sta.ap.ssid, CONFIG_WIFI_SSID, strlen(CONFIG_WIFI_SSID));
    memcpy((char *)wifi_config_sta.ap.password, CONFIG_WIFI_PASSWORD, strlen(CONFIG_WIFI_PASSWORD));

    memset(&wifi_config_ap, 0, sizeof(wifi_config_ap));
    wifi_config_ap.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;

    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    snprintf(buffer, 32, "%s-%02X", CONFIG_AP_WIFI_SSID, mac[5]);
    wifi_config_ap.ap.ssid_len = strlen(buffer);
    memcpy((char *)wifi_config_ap.ap.ssid, buffer, strlen(buffer));
    // wifi_config_ap.ap.ssid_len = strlen(CONFIG_AP_WIFI_SSID);
    // strncpy((char *)wifi_config_ap.ap.ssid, CONFIG_AP_WIFI_SSID, strlen(CONFIG_AP_WIFI_SSID));
    memcpy((char *)wifi_config_ap.ap.password, CONFIG_AP_WIFI_PASSWORD, strlen(CONFIG_AP_WIFI_PASSWORD));
    wifi_config_ap.ap.max_connection = CONFIG_AP_MAX_STA_CONN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap));
    // WiFi.setSleepMode(WIFI_NONE_SLEEP);
    ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "Waiting for wifi");
    // xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, portMAX_DELAY);
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    UNUSED(client);

    // your_context_t *context = event->context;
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

bool is_beacon_bd_addr_set(uint16_t maj, uint16_t min)
{
    uint8_t idx = beacon_maj_min_to_idx(maj, min);

    return ble_beacons[idx].beacon_data.bd_addr_set;
}

void set_beaconaddress(uint16_t maj, uint16_t min, esp_bd_addr_t *p_bd_addr)
{
    uint8_t idx = beacon_maj_min_to_idx(maj, min);

    if ((ble_beacons[idx].beacon_data.bd_addr_set) || (idx == UNKNOWN_BEACON)){
        return;
    }

    memcpy(ble_beacons[idx].beacon_data.bd_addr, *p_bd_addr, sizeof(esp_bd_addr_t));
    ble_beacons[idx].beacon_data.bd_addr_set = true;
}

void update_adv_data(uint16_t maj, uint16_t min, int8_t measured_power,
    float temp, float humidity, uint16_t battery, bool mqtt_send)
{
    uint8_t idx = beacon_maj_min_to_idx(maj, min);

    ble_beacons[idx].adv_data.measured_power = measured_power;
    ble_beacons[idx].adv_data.temp           = temp;
    ble_beacons[idx].adv_data.humidity       = humidity;
    ble_beacons[idx].adv_data.battery        = battery;
    ble_beacons[idx].adv_data.last_seen      = esp_timer_get_time();

    if(mqtt_send){
        ESP_LOGD(TAG, "update_adv_data, update mqtt_last_send");
        ble_beacons[idx].adv_data.mqtt_last_send = esp_timer_get_time();
    }

    display_status.current_beac = UNKNOWN_BEACON;
}

void check_update_display(uint16_t maj, uint16_t min)
{
    uint8_t idx = beacon_maj_min_to_idx(maj, min);

    if( (display_status.current_screen == BEACON_SCREEN) && (display_status.current_beac == idx) ){
        display_status.current_beac = UNKNOWN_BEACON; // invalidate beacon to force update
        return;
    } else if ( (display_status.current_screen == LASTSEEN_SCREEN) ){
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

static void __attribute__((unused)) show_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    ESP_LOGI(TAG, "Bonded devices number : %d", dev_num);
    for (int i = 0; i < dev_num; i++) {
        ESP_LOGI(TAG, "Bond device num %d,", i);
        ESP_LOGI(TAG, "bd_addr");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t), ESP_LOG_INFO);
        ESP_LOGI(TAG, "bond_key.pid_key.irk");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, (void *)dev_list[i].bond_key.pid_key.irk, sizeof(esp_bt_octet16_t), ESP_LOG_INFO);
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

void remove_bonded_devices_num(uint8_t num_bond_device)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
        esp_ble_remove_bond_device(dev_list[num_bond_device].bd_addr);

    free(dev_list);
}

static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_REG_EVT");
        esp_ble_gap_config_local_privacy(true);
        break;
    case ESP_GATTC_CONNECT_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGD(TAG, "REMOTE BDA:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, sizeof(esp_bd_addr_t), ESP_LOG_DEBUG);
        gattc_is_connected = true;
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
        ESP_LOGD(TAG, "open success");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_CFG_MTU_EVT");
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
            ESP_LOGD(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_32){
            ESP_LOGD(TAG, "UUID32: %x", p_data->search_res.srvc_id.uuid.uuid.uuid32);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128){
            esp_gatt_id_t *srvc_id = &p_data->search_res.srvc_id;
            ESP_LOGD(TAG, "UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",       srvc_id->uuid.uuid.uuid128[0],
                     srvc_id->uuid.uuid.uuid128[1], srvc_id->uuid.uuid.uuid128[2], srvc_id->uuid.uuid.uuid128[3],
                     srvc_id->uuid.uuid.uuid128[4], srvc_id->uuid.uuid.uuid128[5], srvc_id->uuid.uuid.uuid128[6],
                     srvc_id->uuid.uuid.uuid128[7], srvc_id->uuid.uuid.uuid128[8], srvc_id->uuid.uuid.uuid128[9],
                     srvc_id->uuid.uuid.uuid128[10], srvc_id->uuid.uuid.uuid128[11], srvc_id->uuid.uuid.uuid128[12],
                     srvc_id->uuid.uuid.uuid128[13], srvc_id->uuid.uuid.uuid128[14], srvc_id->uuid.uuid.uuid128[15]);
            ESP_LOGD(TAG, "service found, start_handle 0x%X end_handle 0x%X", p_data->search_res.start_handle, p_data->search_res.end_handle);
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGD(TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGD(TAG, "Get service information from flash");
        } else {
            ESP_LOGW(TAG, "unknown service source");
        }
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        ESP_LOGD(TAG, "get_server %d", get_server);
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
            ESP_LOGD(TAG, "count %d", count);
            if (count > 0){
                esp_gattc_char_elem_t *char_elem_result = NULL;
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
                        ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with notify", char_elem_result[0].char_handle);
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
                        ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with indicate", char_elem_result[0].char_handle);
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
        ESP_LOGD(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
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
            ESP_LOGD(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT count = %d", count);
            if (count > 0){
                esp_gattc_descr_elem_t *descr_elem_result = NULL;
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

                    ESP_LOGD(TAG, "esp_ble_gattc_write_char_descr (notify) 0x%X", descr_elem_result[0].handle);

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
        // ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT");

        static int64_t time_measure = 0;
        uint8_t len = 0;
        uint16_t count = 0;
        uint8_t idx = gattc_connect_beacon_idx;

        if (p_data->notify.is_notify){
            count = ble_beacons[idx].offline_buffer_count;
            if(!count){
                ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, first, idx %d, ble_beacons[idx] %d, ble_beacons[idx].p_buffer_download[count] %d",
                    idx, (&ble_beacons[idx] != NULL),  (ble_beacons[idx].p_buffer_download != NULL) );
                time_measure = esp_timer_get_time();
            } else {
                ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, subsequent, count %d", count);
            }
            ble_beacons[idx].p_buffer_download[count].sequence_number  = uint16_decode(&p_data->notify.value[len]); len += 2; // uint16_t
            ble_beacons[idx].p_buffer_download[count].time_stamp       = uint32_decode(&p_data->notify.value[len]); len += 4; // time_t
            ble_beacons[idx].p_buffer_download[count].temperature_f
                = SHT3_GET_TEMPERATURE_VALUE(p_data->notify.value[len], p_data->notify.value[len+1]);
            len += 2;
            ble_beacons[idx].p_buffer_download[count].humidity_f
                = SHT3_GET_HUMIDITY_VALUE(p_data->notify.value[len], p_data->notify.value[len+1]);
            len += 2;
            // ESP_LOG_BUFFER_HEX_LEVEL(TAG, &ble_beacons[idx].p_buffer_download[count], p_data->notify.value_len, ESP_LOG_DEBUG);
            ble_beacons[idx].offline_buffer_count++;
        } else {
            ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT done, received %d, time difference %d",
                ble_beacons[idx].offline_buffer_count, (uint16_t) (esp_timer_get_time() - time_measure));
        }

        if (!p_data->notify.is_notify){
            ESP_LOGD(TAG, "now disconnecting");
            esp_gatt_status_t ret_status = esp_ble_gattc_close(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, 0);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_close, error status %d", ret_status);
            }
            ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE;
            gattc_offline_buffer_downloading = false;
        }
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGD(TAG, "write descr success handle 0x%X", p_data->write.handle);
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

        ESP_LOGD(TAG, "esp_ble_gattc_write_char 0x%X", gl_profile_tab[PROFILE_A_APP_ID].char_handle_2A52);
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                  gl_profile_tab[PROFILE_A_APP_ID].char_handle_2A52,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NO_MITM);       // ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGD(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, bda, sizeof(esp_bd_addr_t), ESP_LOG_DEBUG);
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write char failed, error status = %x, error = %s", p_data->write.status, esp_err_to_name(p_data->write.status) );
            break;
        }
        ESP_LOGD(TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_DISCONNECT_EVT");
        if(gattc_offline_buffer_downloading) {
            if(gattc_give_up_now == true){
                free_offline_buffer(gattc_connect_beacon_idx, OFFLINE_BUFFER_STATUS_NONE);
                gattc_give_up_now = false;
                gattc_offline_buffer_downloading = false;
            } else {
                // disconnect occured but download not completed, maybe next time
                reset_offline_buffer(gattc_connect_beacon_idx, OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED);
                gattc_offline_buffer_downloading = false;
            }
        }

        gattc_connect = false;
        gattc_is_connected = false;
        gattc_connect_beacon_idx = UNKNOWN_BEACON;
        gattc_give_up_now = false;
        get_server = false;
        device_notify_1401 = false;
        device_indicate_2A52 = false;

        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);

        uint32_t duration = 0;  // scan permanently
        esp_ble_gap_start_scanning(duration);
        break;
    default:
        break;
    }
}

static void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGI(GATTS_TAG, "REGISTER_APP_EVT, status %d, app_id %d\n", param->reg.status, param->reg.app_id);
        gl_profile_s_tab[PROFILE_A_APP_S_ID].service_id.is_primary = true;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].service_id.id.inst_id = 0x00;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_CTS;
        esp_ble_gatts_create_service(gatts_if, &gl_profile_s_tab[PROFILE_A_APP_ID].service_id, GATTS_NUM_HANDLE_CTS);
        break;
    case ESP_GATTS_READ_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_READ_EVT, conn_id %d, trans_id %d, handle %d\n", param->read.conn_id, param->read.trans_id, param->read.handle);
        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 10;

        /**< |     Year        |Month   |Day     |Hours   |Minutes |Seconds |Weekday |Fraction|Reason  |
             |     2 bytes     |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  | = 10 bytes.

          Example: C2 07 0B 0F 0D 25 2A 06 FE 08
                0x07C2  1986
                0x0B    11
                0x0F    15
                0x0D    13
                0x25    37
                0x2A    42
                0x06    Saturday
                0xFE    254/256
                0x08    Daylight savings 1, Time zone 0, External update 0, Manual update 0
        **/

        rsp.attr_value.value[0] = 0xC2;
        rsp.attr_value.value[1] = 0x07;
        rsp.attr_value.value[2] = 0x0B;
        rsp.attr_value.value[3] = 0x0F;
        rsp.attr_value.value[4] = 0x0D;
        rsp.attr_value.value[5] = 0x25;
        rsp.attr_value.value[6] = 0x2A;
        rsp.attr_value.value[7] = 0x06;
        rsp.attr_value.value[8] = 0xFE;
        rsp.attr_value.value[9] = 0x08;
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGI(GATTS_TAG, "GATT_WRITE_EVT, conn_id %d, trans_id %d, handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGI(GATTS_TAG,"ESP_GATTS_EXEC_WRITE_EVT");
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGI(GATTS_TAG, "CREATE_SERVICE_EVT, status %d,  service_handle %d\n", param->create.status, param->create.service_handle);
        gl_profile_s_tab[PROFILE_A_APP_S_ID].service_handle = param->create.service_handle;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_CTS;

        esp_ble_gatts_start_service(gl_profile_s_tab[PROFILE_A_APP_S_ID].service_handle);
        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_s_tab[PROFILE_A_APP_S_ID].service_handle, &gl_profile_s_tab[PROFILE_A_APP_S_ID].char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                                        &gatts_cts_char1_val, NULL);
        if (add_char_ret){
            ESP_LOGE(GATTS_TAG, "add char failed, error code =%x",add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        uint16_t length = 0;
        const uint8_t *prf_char;

        ESP_LOGI(GATTS_TAG, "ADD_CHAR_EVT, status %d,  attr_handle %d, service_handle %d\n",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        gl_profile_s_tab[PROFILE_A_APP_S_ID].char_handle = param->add_char.attr_handle;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_s_tab[PROFILE_A_APP_S_ID].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
        if (get_attr_ret == ESP_FAIL){
            ESP_LOGE(GATTS_TAG, "ILLEGAL HANDLE");
        }

        ESP_LOGI(GATTS_TAG, "the gatts demo char length = %x\n", length);
        for(int i = 0; i < length; i++){
            ESP_LOGI(GATTS_TAG, "prf_char[%x] =%x\n",i,prf_char[i]);
        }
        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_s_tab[PROFILE_A_APP_ID].service_handle, &gl_profile_s_tab[PROFILE_A_APP_ID].descr_uuid,
                                                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        if (add_descr_ret){
            ESP_LOGE(GATTS_TAG, "add char descr failed, error code =%x", add_descr_ret);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile_s_tab[PROFILE_A_APP_ID].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGI(GATTS_TAG, "ADD_DESCR_EVT, status %d, attr_handle %d, service_handle %d\n",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGI(GATTS_TAG, "SERVICE_START_EVT, status %d, service_handle %d\n",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;    // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;    // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;    // timeout = 400*10ms = 4000ms
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_s_tab[PROFILE_A_APP_S_ID].conn_id = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGI(GATTS_TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK){
            esp_log_buffer_hex(GATTS_TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
    case ESP_GATTS_CANCEL_OPEN_EVT:
    case ESP_GATTS_CLOSE_EVT:
    case ESP_GATTS_LISTEN_EVT:
    case ESP_GATTS_CONGEST_EVT:
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT");
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
        ESP_LOGD(TAG, "scan start success");
        gattc_scanning = true;
        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        /* Call the following function to input the passkey which is displayed on the remote device */
        //esp_ble_passkey_reply(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, true, 0x00);
        ESP_LOGD(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
        esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
    }
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
        break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should send the security response with negative(false) accept value*/
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_NC_REQ_EVT");
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        ESP_LOGD(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
        ESP_LOGD(TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT");
        ///show the passkey number to the user to input it in the peer device.
        ESP_LOGI(TAG, "The passkey Notify number:%06d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_KEY_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_KEY_EVT");
        //shows the ble key info share with peer device to the user.
        ESP_LOGD(TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_AUTH_CMPL_EVT");
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

        ESP_LOGD(TAG, "esp_ble_gattc_search_service");
        // esp_ble_gattc_search_service(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, gl_profile_tab[PROFILE_A_APP_ID].conn_id, &remote_filter_service_uuid);

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
        // ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;

        beacon_type_t beacon_type = UNKNOWN_BEACON;
        uint16_t maj = 0, min = 0, battery = 0;
        int16_t x = 0, y = 0, z = 0;
        uint8_t idx = 0;
        float temp = 0, humidity = 0;

        bool is_beacon_active = true;
        bool is_beacon_close = false;
        bool mqtt_send_adv = false;

        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            // ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0) {
                ESP_LOGD(TAG, "adv data:");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len, ESP_LOG_DEBUG);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0) {
                ESP_LOGD(TAG, "scan resp:");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len, ESP_LOG_DEBUG);
            }
#endif

            beacon_type = esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);

            if( (beacon_type == BEACON_V3) || (beacon_type == BEACON_V4) || (beacon_type == BEACON_V4_SR) ){

                // ESP_LOGD(TAG, "mybeacon found, type %d", beacon_type);
                switch(beacon_type){

                case BEACON_V3: {
                    decode_mybeacon_packet_v3((esp_ble_mybeacon_v3_t*)(scan_result->scan_rst.ble_adv), &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active, &is_beacon_close);
                    break;
                }

                case BEACON_V4:
                case BEACON_V4_SR:
                {
                    esp_ble_mybeacon_payload_t *mybeacon_payload = (esp_ble_mybeacon_payload_t *)(&scan_result->scan_rst.ble_adv[11]);

                    decode_mybeacon_packet_v4(mybeacon_payload, scan_result->scan_rst.ble_adv, &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active, &is_beacon_close);
                    break;
                }
                default:
                    ESP_LOGE(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: should not happen");
                    break;
                }

                ESP_LOGD(TAG, "(0x%04x%04x) rssi %3d, found, is_beacon_close %d, display_message_is_shown %d, idx %d",
                    maj, min, scan_result->scan_rst.rssi, is_beacon_close, display_status.display_message_is_shown, idx);

                if(is_beacon_close){
                    if(display_status.display_message_is_shown){
                        oneshot_display_message_timer_touch();
                    } else {
                        display_message_content.beac = idx;
                        snprintf(display_message_content.title, 32, "Beacon Identified");
                        snprintf(display_message_content.message, 32, "Name: %s", ble_beacons[idx].beacon_data.name);
                        snprintf(display_message_content.comment, 32, "Activated: %c", (is_beacon_idx_active(idx)? 'y':'n'));
                        snprintf(display_message_content.action, 32, "%s", "Toggle active w/Button");
                        display_message_show();
                    }
                }

                if(!is_beacon_active){
                    break;
                }

#if CONFIG_USE_MQTT==1
                mqtt_send_adv = send_to_mqtt(idx, maj, min, temp, humidity, battery, scan_result->scan_rst.rssi);
#endif

                ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d | mqtt send %c",
                    maj, min, scan_result->scan_rst.rssi, temp, humidity, x, y, z, battery, (mqtt_send_adv ? 'y':'n') );

                if(!is_beacon_bd_addr_set(maj, min)){
                    set_beaconaddress(maj, min, &scan_result->scan_rst.bda);
                }

                update_adv_data(maj, min, scan_result->scan_rst.rssi, temp, humidity, battery, mqtt_send_adv);
                check_update_display(maj, min);

                if ((ble_beacons[idx].offline_buffer_status == OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED) && (gattc_connect == false)) {
                    if(gattc_give_up_now == false){
                        gattc_connect = true;
                        gattc_connect_beacon_idx = idx;
                        ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS;
                        gattc_offline_buffer_downloading = true;

                        ESP_LOGD(TAG, "connect to the remote device.");
                        esp_ble_gap_stop_scanning();
                        esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    } else {
                        free_offline_buffer(idx, OFFLINE_BUFFER_STATUS_NONE);
                        gattc_give_up_now = false;
                    }
                }
            } else {
                // ESP_LOGD(TAG, "mybeacon not found");
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
            break;
        default:
            ESP_LOGE(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT - default, evt %d", event);
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
            ESP_LOGD(TAG, "Stop scan successfully");
        }
        gattc_scanning = false;
        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGD(TAG, "Stop adv successfully");
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

static void esp_gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_s_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGI(GATTS_TAG, "Reg app failed, app_id %04x, status %d\n",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    ESP_LOGI(TAG, "esp_gatts_cb: evt %d", event);

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE||gatts_if == gl_profile_s_tab[idx].gatts_if) 			{
                if (gl_profile_s_tab[idx].gatts_cb) {
                    gl_profile_s_tab[idx].gatts_cb(event, gatts_if, param);
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
        ESP_LOGI(TAG, "save_blemqttproxy_param: save param ok, beacon mask %u", blemqttproxy_param.active_beacon_mask);
    } else {
        ESP_LOGE(TAG, "save_blemqttproxy_param: save param failed, ret = %d, initialize and save to NVS", ret);
        blemqttproxy_param.active_beacon_mask = s_active_beacon_mask;
        ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    }

    s_active_beacon_mask = blemqttproxy_param.active_beacon_mask & mask;

    ESP_LOGI(TAG, "save_blemqttproxy_param: blemqttproxy_param.active_beacon_mask = %d", s_active_beacon_mask);

    return ret;
}

#if ((CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1) || (CONFIG_WDT_REBOOT_LAST_SEND_THRESHOLD==1) || (CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1) )
static void wdt_task(void* pvParameters)
{
    EventBits_t uxBits;
    char buffer[32], buffer_topic[32], buffer_payload[32];
    uint32_t uptime_sec;
    int msg_id = 0; UNUSED(msg_id);
    periodic_wdt_timer_start();

    while (1) {
        uxBits = xEventGroupWaitBits(s_wdt_evg, UPDATE_ESP_RESTART | UPDATE_ESP_MQTT_RESTART, pdTRUE, pdFALSE, portMAX_DELAY);

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

void adjust_log_level()
{
    esp_log_level_set("wpa", ESP_LOG_WARN);
    esp_log_level_set("esp_timer", ESP_LOG_WARN);
    esp_log_level_set("httpd", ESP_LOG_WARN);
    esp_log_level_set("httpd_sess", ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx", ESP_LOG_WARN);
    esp_log_level_set("httpd_parse", ESP_LOG_WARN);
    esp_log_level_set("httpd_uri", ESP_LOG_INFO);
    esp_log_level_set("HTTP_CLIENT", ESP_LOG_INFO);
    esp_log_level_set("phy", ESP_LOG_WARN);
    esp_log_level_set("phy_init", ESP_LOG_WARN);
    esp_log_level_set("esp_image", ESP_LOG_WARN);
    esp_log_level_set("tcpip_adapter", ESP_LOG_WARN);
    esp_log_level_set("efuse", ESP_LOG_WARN);
    esp_log_level_set("nvs", ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT", ESP_LOG_INFO);
    esp_log_level_set("OUTBOX", ESP_LOG_INFO);
    esp_log_level_set("memory_layout", ESP_LOG_WARN);
    esp_log_level_set("heap_init", ESP_LOG_WARN);
    esp_log_level_set("intr_alloc", ESP_LOG_WARN);
    esp_log_level_set("esp_ota_ops", ESP_LOG_WARN);
    esp_log_level_set("boot_comm", ESP_LOG_WARN);
    esp_log_level_set("wifi", ESP_LOG_INFO);
    esp_log_level_set("BT_BTM", ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_INFO);
    esp_log_level_set("timer", ESP_LOG_INFO);
    esp_log_level_set("beacon", ESP_LOG_INFO);
    esp_log_level_set("display", ESP_LOG_INFO);
    esp_log_level_set("event", ESP_LOG_INFO);
    esp_log_level_set("ble_mqtt", ESP_LOG_INFO);
    esp_log_level_set("web_file_server", ESP_LOG_INFO);
    esp_log_level_set("BLEMQTTPROXY", ESP_LOG_DEBUG);
}

void initialize_ble()
{
    remote_filter_service_uuid.len = ESP_UUID_LEN_128;
    memcpy(remote_filter_service_uuid.uuid.uuid128, REMOTE_SERVICE_UUID, 16);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(esp_gatts_cb));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(PROFILE_A_APP_ID));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_A_APP_S_ID));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}

void initialize_ble_security()
{
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
}

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
SemaphoreHandle_t xGuiSemaphore;

static void IRAM_ATTR lv_tick_task(void* arg) {
	lv_tick_inc(portTICK_RATE_MS);
}

void initialize_lv()
{
    lv_init();
    lvgl_driver_init();
    static lv_color_t buf1[DISP_BUF_SIZE];
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    static lv_color_t buf2[DISP_BUF_SIZE];
#endif
    static lv_disp_buf_t disp_buf;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);
#else
    lv_disp_buf_init(&disp_buf, buf1, NULL, DISP_BUF_SIZE);
#endif

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.rounder_cb = disp_driver_rounder;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306
    disp_drv.set_px_cb = ssd1306_set_px_cb;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
    disp_drv.set_px_cb = sh1107_set_px_cb;
#endif
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
}

static void gui_prepare()
{
    xGuiSemaphore = xSemaphoreCreateMutex();

    initialize_lv();

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_task,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic_gui"
    };
    static esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10*1000)); //10ms (expressed as microseconds)

    lv_init_screens();
}

static void gui_task(void* pvParameters)
{
    display_create_timer();
    gui_prepare();
    ESP_LOGD(TAG, "gui_task, gui_prepare() done"); fflush(stdout);

    lv_task_create(update_display_task, 100, LV_TASK_PRIO_MID, NULL);

    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff
        if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
    vTaskDelete(NULL);
}

static void initialize_buttons()
{
    for (int i = 0; i < CONFIG_BUTTON_COUNT; i++){
        btn_handle[i] = iot_button_create(btn[i][1], BUTTON_ACTIVE_LEVEL);
        iot_button_set_evt_cb(btn_handle[i], BUTTON_CB_PUSH, button_push_cb, &btn[i][0]);
        iot_button_set_evt_cb(btn_handle[i], BUTTON_CB_RELEASE, button_release_cb, &btn[i][0]);
    }
}

void app_main()
{
    adjust_log_level();

    // NVS initialization and beacon mask retrieval
    initialize_nvs();
    ESP_ERROR_CHECK(read_blemqttproxy_param());
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    wifi_evg = xEventGroupCreate();
    mqtt_evg = xEventGroupCreate();
    s_wdt_evg = xEventGroupCreate();
    ota_evg = xEventGroupCreate();

    initialize_buttons();

    xTaskCreatePinnedToCore(gui_task, "gui_task", 4096*2, NULL, 0, NULL, 1);

    create_timer();

    oneshot_timer_usage = TIMER_SPLASH_SCREEN;

    ESP_LOGI(TAG, "app_main, start oneshot timer, %d", SPLASH_SCREEN_TIMER_DURATION);
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, SPLASH_SCREEN_TIMER_DURATION));

    initialize_ble();
    initialize_ble_security();

    vTaskDelay(pdMS_TO_TICKS(10));

    xTaskCreate(&wifi_mqtt_task, "wifi_mqtt_task", 2048 * 2, NULL, 5, NULL);

#if (CONFIG_WDT_REBOOT_LAST_SEEN_THRESHOLD==1 || CONFIG_WDT_SEND_MQTT_BEFORE_REBOOT==1)
    xTaskCreate(&wdt_task, "wdt_task", 2048 * 2, NULL, 5, NULL);
#endif

    xTaskCreate(&ota_task, "ota_task", 8192, NULL, 5, NULL);
}
