#ifndef __BLE_H__
#define __BLE_H__

#include "esp_gap_ble_api.h"

#define INVALID_HANDLE   0

void show_bonded_devices(void);
void remove_all_bonded_devices(void);
void remove_bonded_devices_num(uint8_t num_bond_device);

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

void initialize_ble();
void initialize_ble_security();

#endif // __BLE_H__
