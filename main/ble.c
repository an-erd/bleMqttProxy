#include "esp_gatt_defs.h"
#include "ble.h"

const uint8_t const REMOTE_SERVICE_UUID[] = {0x3C, 0xB7, 0xA2, 0x4B, 0x0C, 0x32, 0xF2, 0x9F, 0x4F, 0x4C, 0xF5, 0x37, 0x00, 0x14, 0x2F, 0x61};
const char remote_device_name[20] = "";
bool gattc_connect = false;
bool get_server = false;
bool device_notify_1401 = false;
bool device_indicate_2A52 = false;

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
