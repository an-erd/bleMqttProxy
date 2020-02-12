#ifndef __BLE_H__
#define __BLE_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"

// BLE
extern const uint8_t const REMOTE_SERVICE_UUID[];
#define REMOTE_NOTIFY_CHAR_UUID         0x1401
#define REMOTE_INDICATION_CHAR_UUID     0x2A52
#define REMOTE_NOTIFY_HANDLE            0x26
#define REMOTE_INDICATE_HANDLE          0x2B

#define PROFILE_NUM      2
#define PROFILE_A_APP_ID 0
#define PROFILE_B_APP_ID 1
#define INVALID_HANDLE   0

extern bool gattc_connect;
extern uint8_t gattc_connect_beacon_idx;
extern bool get_server;
extern bool device_notify_1401;
extern bool device_indicate_2A52;

extern esp_bt_uuid_t remote_filter_service_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_1401;
extern esp_bt_uuid_t notify_descr_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_2A52;
extern esp_bt_uuid_t indication_descr_uuid;

#endif // __BLE_H__
