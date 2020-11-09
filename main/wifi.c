#include "blemqttproxy.h"

#include "wifi.h"
#include "stats.h"
#include "web_file_server.h"
#include "ble_mqtt.h"
#include "ble_sntp.h"

static const char* TAG = "WIFI";

EventGroupHandle_t wifi_evg;
tcpip_adapter_ip_info_t ipinfo;
uint8_t mac[6];

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
                initialize_sntp();
                }
                break;
            default:
                break;
        }
    }

    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    // refactor
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

void wifi_init(void *arg)
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
    memcpy((char *)wifi_config_ap.ap.password, CONFIG_AP_WIFI_PASSWORD, strlen(CONFIG_AP_WIFI_PASSWORD));
    wifi_config_ap.ap.max_connection = CONFIG_AP_MAX_STA_CONN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config_sta));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config_ap));
    ESP_LOGI(TAG, "start the WIFI SSID:[%s]", CONFIG_WIFI_SSID);
    ESP_ERROR_CHECK(esp_wifi_start());
}

void wifi_mqtt_task(void* pvParameters)
{
    wifi_init(&web_server);
    mqtt_init();
    vTaskDelete(NULL);
}
