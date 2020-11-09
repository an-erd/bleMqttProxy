#ifndef __BLE_MYCLIENT_H__
#define __BLE_MYCLIENT_H__

#include "esp_bt_defs.h"
#include "esp_gatt_defs.h"
#include "esp_gattc_api.h"

#include "ble.h"
#define PROFILE_ID_BEACON                       0

extern bool gattc_connect;
extern bool gattc_is_connected;
extern bool gattc_scanning;
extern bool gattc_give_up_now;
extern uint8_t gattc_connect_beacon_idx;

const uint8_t REMOTE_SERVICE_UUID[16];

extern bool get_server;
extern bool device_notify_1401;
extern bool device_indicate_2A52;
extern bool gattc_offline_buffer_downloading;


extern esp_bt_uuid_t remote_filter_service_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_1401;
extern esp_bt_uuid_t notify_descr_uuid;
extern esp_bt_uuid_t remote_filter_char_uuid_2A52;
extern esp_bt_uuid_t indication_descr_uuid;

void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);

/* Link the gatts, gap and gattc functions */
void gattc_search_service_beacon();
void gattc_open_beacon();

#endif // __BLE_MYCLIENT_H__
