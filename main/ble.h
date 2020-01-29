#ifndef __BLE_H__
#define __BLE_H__

// UUIDs from nRF52 SDK and ble_beacon implementation
#define BLE_UUID_OUR_SERVICE                            0x1400
#define BLE_UUID_OUR_SERVICE_MEASUREMENT_CHAR           0x1401
#define BLE_UUID_RECORD_ACCESS_CONTROL_POINT_CHAR       0x2A52

#define REMOTE_SERVICE_UUID         (BLE_UUID_OUR_SERVICE)
#define REMOTE_NOTIFY_CHAR_UUID     (BLE_UUID_OUR_SERVICE_MEASUREMENT_CHAR)
#define REMOTE_INDICATE_CHAR_UUID   (BLE_UUID_RECORD_ACCESS_CONTROL_POINT_CHAR)
#define PROFILE_NUM      1
#define PROFILE_A_APP_ID 0
#define INVALID_HANDLE   0
#define SECOND_TO_USECOND          1000000

static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result   = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;
static bool is_connected = false;

static esp_bt_uuid_t remote_filter_service_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_SERVICE_UUID,},
};

static esp_bt_uuid_t remote_filter_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = REMOTE_NOTIFY_CHAR_UUID,},
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};


#endif // __BLE_H__
