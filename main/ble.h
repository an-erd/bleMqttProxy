#ifndef __BLE_H__
#define __BLE_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "esp_bt_defs.h"
#include "esp_gatt_common_api.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_defs.h"
#include "esp_bt_main.h"

#include "offlinebuffer.h"

// BLE
#define INVALID_HANDLE   0
#define PROFILE_NUM      1

// GATTC
#define PROFILE_A_APP_ID                0
extern const uint8_t REMOTE_SERVICE_UUID[];
#define REMOTE_NOTIFY_CHAR_UUID         0x1401
#define REMOTE_INDICATION_CHAR_UUID     0x2A52
#define REMOTE_NOTIFY_HANDLE            0x26
#define REMOTE_INDICATE_HANDLE          0x2B

// GATTS
#define PROFILE_A_APP_S_ID              0
#define TEST_DEVICE_NAME            "ESP_GATTS_DEMO"
#define TEST_MANUFACTURER_DATA_LEN  17

#define BLE_SVC_CTS_UUID16


#define GATTS_SERVICE_UUID_CTS          0x1805
#define GATTS_CHAR_UUID_CTS             0x2A2B      //BLE_SVC_CTS_CHR_UUID16_LOCAL_TIME_INFORMATION
#define GATTS_NUM_HANDLE_CTS            4

#define GATTS_CTS_CHAR_VAL_LEN_MAX 0x40
extern esp_attr_value_t gatts_cts_char1_val;

extern bool gattc_connect;
extern bool gattc_is_connected;
extern bool gattc_scanning;
extern bool gattc_give_up_now;

extern uint8_t gattc_connect_beacon_idx;
extern bool get_server;
extern bool device_notify_1401;
extern bool device_indicate_2A52;
extern bool gattc_offline_buffer_downloading;

extern esp_bt_uuid_t remote_filter_service_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_1401;
extern esp_bt_uuid_t notify_descr_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_2A52;
extern esp_bt_uuid_t indication_descr_uuid;

void alloc_offline_buffer(uint8_t idx, offline_buffer_status_t status);
void free_offline_buffer(uint8_t idx, offline_buffer_status_t status);
void reset_offline_buffer(uint8_t idx, offline_buffer_status_t status);

#endif // __BLE_H__
