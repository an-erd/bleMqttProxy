#include "blemqttproxy.h"

/* bleMqttProxy specific */
#include "helperfunctions.h"
#include "display.h"
#include "ble.h"
#include "beacon.h"
#include "ble_mqtt.h"
#include "timer.h"
#include "web_file_server.h"
#include "offlinebuffer.h"
#include "ota.h"
#include "params.h"
#include "iot_button.h"
#include "wifi.h"
#include "watchdog.h"
#include "timer.h"
#include "gui.h"
#include "buttons.h"

static const char* TAG = "BLEMQTTPROXY";

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

void adjust_log_level()
{
    esp_log_level_set("wpa",                ESP_LOG_WARN);
    esp_log_level_set("esp_timer",          ESP_LOG_WARN);
    esp_log_level_set("httpd",              ESP_LOG_WARN);
    esp_log_level_set("httpd_sess",         ESP_LOG_WARN);
    esp_log_level_set("httpd_txrx",         ESP_LOG_WARN);
    esp_log_level_set("httpd_parse",        ESP_LOG_WARN);
    esp_log_level_set("httpd_uri",          ESP_LOG_WARN);
    esp_log_level_set("HTTP_CLIENT",        ESP_LOG_WARN);
    esp_log_level_set("phy",                ESP_LOG_WARN);
    esp_log_level_set("phy_init",           ESP_LOG_WARN);
    esp_log_level_set("esp_image",          ESP_LOG_WARN);
    esp_log_level_set("tcpip_adapter",      ESP_LOG_WARN);
    esp_log_level_set("efuse",              ESP_LOG_WARN);
    esp_log_level_set("nvs",                ESP_LOG_WARN);
    esp_log_level_set("BTDM_INIT",          ESP_LOG_WARN);
    esp_log_level_set("OUTBOX",             ESP_LOG_WARN);
    esp_log_level_set("memory_layout",      ESP_LOG_WARN);
    esp_log_level_set("heap_init",          ESP_LOG_WARN);
    esp_log_level_set("intr_alloc",         ESP_LOG_WARN);
    esp_log_level_set("esp_ota_ops",        ESP_LOG_WARN);
    esp_log_level_set("boot_comm",          ESP_LOG_WARN);
    esp_log_level_set("wifi",               ESP_LOG_INFO);
    esp_log_level_set("BT_BTM",             ESP_LOG_INFO);
    esp_log_level_set("MQTT_CLIENT",        ESP_LOG_INFO);
    esp_log_level_set("event",              ESP_LOG_INFO);

    esp_log_level_set("BEACON",             ESP_LOG_INFO);
    esp_log_level_set("BLE_MQTT",           ESP_LOG_INFO);
    esp_log_level_set("BLE_MYCLIENT",       ESP_LOG_DEBUG);
    esp_log_level_set("BLE_MYSERVER",       ESP_LOG_DEBUG);
    esp_log_level_set("BLE_SNTP",           ESP_LOG_INFO);
    esp_log_level_set("BLE",                ESP_LOG_DEBUG);
    esp_log_level_set("BLEMQTTPROXY",       ESP_LOG_INFO);
    esp_log_level_set("BUTTONS",            ESP_LOG_INFO);
    esp_log_level_set("DISPLAY",            ESP_LOG_INFO);
    esp_log_level_set("GUI",                ESP_LOG_INFO);
    esp_log_level_set("ONLINECUFFER",       ESP_LOG_INFO);
    esp_log_level_set("OTA",                ESP_LOG_INFO);
    esp_log_level_set("PARAMS",             ESP_LOG_INFO);
    esp_log_level_set("STATS",              ESP_LOG_INFO);
    esp_log_level_set("TIMER",              ESP_LOG_INFO);
    esp_log_level_set("WATCHDOG",           ESP_LOG_DEBUG);
    esp_log_level_set("WEB_FILE_SERVER",    ESP_LOG_INFO);
    esp_log_level_set("WIFI",               ESP_LOG_INFO);
}

void create_eventgroups()
{
    wifi_evg = xEventGroupCreate();
    mqtt_evg = xEventGroupCreate();
    wdt_evg = xEventGroupCreate();
    ota_evg = xEventGroupCreate();
}

void app_main()
{
    ESP_LOGI(TAG, "app_main() >");
    adjust_log_level();
    initialize_nvs();
    ESP_ERROR_CHECK(read_blemqttproxy_param());
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
    create_eventgroups();
    initialize_buttons();
    xTaskCreatePinnedToCore(gui_task, "gui_task", 5 * 512, NULL, 0, NULL, 1);
    create_timer();
    oneshot_timer_usage = TIMER_SPLASH_SCREEN;
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, SPLASH_SCREEN_TIMER_DURATION));
    initialize_ble();
    initialize_ble_security();
    xTaskCreate(&wifi_mqtt_task, "wifi_mqtt_task", 5 * 512, NULL, 5, NULL);
    xTaskCreate(&wdt_task, "wdt_task", 3 * 256, NULL, 5, NULL);
    xTaskCreate(&ota_task, "ota_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "app_main() <");
}
