#ifndef __BLE_MYSERVER_H__
#define __BLE_MYSERVER_H__

#include "esp_gatts_api.h"
#include "esp_bt_defs.h"

#define PROFILE_ID_CTS                  0

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);
void esp_gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param);

#endif // __BLE_MYSERVER_H__