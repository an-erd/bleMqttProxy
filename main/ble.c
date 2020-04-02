#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "ble.h"

#include "beacon.h"
static const char *TAG = "ble";

const uint8_t const REMOTE_SERVICE_UUID[] = {0x3C, 0xB7, 0xA2, 0x4B, 0x0C, 0x32, 0xF2, 0x9F, 0x4F, 0x4C, 0xF5, 0x37, 0x00, 0x14, 0x2F, 0x61};
const char remote_device_name[20] = "";
bool gattc_connect = false;
bool gattc_is_connected = false;
bool gattc_scanning = false;
bool gattc_give_up_now = false;
uint8_t gattc_connect_beacon_idx = UNKNOWN_BEACON;

bool get_server = false;
bool device_notify_1401 = false;
bool device_indicate_2A52 = false;

bool gattc_offline_buffer_downloading = false;

esp_bt_uuid_t remote_filter_service_uuid;

esp_bt_uuid_t remote_filter_char_uuid_1401 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
};

esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

esp_bt_uuid_t remote_filter_char_uuid_2A52 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_INDICATION_CHAR_UUID,},
};

esp_bt_uuid_t indication_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

void alloc_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ESP_LOGD(TAG, "alloc_offline_buffer free heap() %d, needed %d", esp_get_free_heap_size(), sizeof(ble_os_meas_t) * CONFIG_OFFLINE_BUFFER_SIZE);
    ble_beacons[idx].p_buffer_download = (ble_os_meas_t *) malloc(sizeof(ble_os_meas_t) * CONFIG_OFFLINE_BUFFER_SIZE);
    ESP_LOGD(TAG, "alloc_offline_buffer p_buffer_download %d ", ble_beacons[idx].p_buffer_download != NULL);
    ble_beacons[idx].offline_buffer_count = 0;
    ble_beacons[idx].offline_buffer_status = status;
}

void free_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ble_beacons[idx].offline_buffer_status = status;
    ble_beacons[idx].offline_buffer_count = 0;
    free(ble_beacons[idx].p_buffer_download);
}

void reset_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ble_beacons[idx].offline_buffer_status = status;
    ble_beacons[idx].offline_buffer_count = 0;
}
