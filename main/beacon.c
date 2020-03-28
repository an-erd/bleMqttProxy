
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "esp_timer.h"
#include "esp_log.h"

#include "beacon.h"
#include "helperfunctions.h"
#include "display.h"

static const char *TAG = "beacon";
extern esp_err_t save_blemqttproxy_param();

esp_ble_mybeacon_head_t mybeacon_common_head_v3 = {
    .flags = {0x02, 0x01, 0x04},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x1502
};

esp_ble_mybeacon_head_t mybeacon_common_head_v4 = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x13,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x0700
};

esp_ble_mybeacon_vendor_t mybeacon_common_vendor_v3 = {
    .proximity_uuid ={CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4},
};

ble_beacon_t ble_beacons[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] =
{
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_1_MAJ,  CONFIG_BLE_DEVICE_1_MIN,  CONFIG_BLE_DEVICE_1_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_2_MAJ,  CONFIG_BLE_DEVICE_2_MIN,  CONFIG_BLE_DEVICE_2_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_3_MAJ,  CONFIG_BLE_DEVICE_3_MIN,  CONFIG_BLE_DEVICE_3_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_4_MAJ,  CONFIG_BLE_DEVICE_4_MIN,  CONFIG_BLE_DEVICE_4_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_5_MAJ,  CONFIG_BLE_DEVICE_5_MIN,  CONFIG_BLE_DEVICE_5_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_6_MAJ,  CONFIG_BLE_DEVICE_6_MIN,  CONFIG_BLE_DEVICE_6_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_7_MAJ,  CONFIG_BLE_DEVICE_7_MIN,  CONFIG_BLE_DEVICE_7_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_8_MAJ,  CONFIG_BLE_DEVICE_8_MIN,  CONFIG_BLE_DEVICE_8_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_9_MAJ,  CONFIG_BLE_DEVICE_9_MIN,  CONFIG_BLE_DEVICE_9_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
    {{{CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_10_MAJ, CONFIG_BLE_DEVICE_10_MIN, CONFIG_BLE_DEVICE_10_NAME, {0,0,0,0,0,0,}, false}, {0}, OFFLINE_BUFFER_STATUS_NONE, 0, 0},
};

uint16_t s_active_beacon_mask = 0;

uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min)
{
    if( (maj == CONFIG_BLE_DEVICE_1_MAJ ) && (min == CONFIG_BLE_DEVICE_1_MIN ) ) return 0;
    if( (maj == CONFIG_BLE_DEVICE_2_MAJ ) && (min == CONFIG_BLE_DEVICE_2_MIN ) ) return 1;
    if( (maj == CONFIG_BLE_DEVICE_3_MAJ ) && (min == CONFIG_BLE_DEVICE_3_MIN ) ) return 2;
    if( (maj == CONFIG_BLE_DEVICE_4_MAJ ) && (min == CONFIG_BLE_DEVICE_4_MIN ) ) return 3;
    if( (maj == CONFIG_BLE_DEVICE_5_MAJ ) && (min == CONFIG_BLE_DEVICE_5_MIN ) ) return 4;
    if( (maj == CONFIG_BLE_DEVICE_6_MAJ ) && (min == CONFIG_BLE_DEVICE_6_MIN ) ) return 5;
    if( (maj == CONFIG_BLE_DEVICE_7_MAJ ) && (min == CONFIG_BLE_DEVICE_7_MIN ) ) return 6;
    if( (maj == CONFIG_BLE_DEVICE_8_MAJ ) && (min == CONFIG_BLE_DEVICE_8_MIN ) ) return 7;
    if( (maj == CONFIG_BLE_DEVICE_9_MAJ ) && (min == CONFIG_BLE_DEVICE_9_MIN ) ) return 8;
    if( (maj == CONFIG_BLE_DEVICE_10_MAJ) && (min == CONFIG_BLE_DEVICE_10_MIN) ) return 9;

    ESP_LOGE(TAG, "beacon_maj_min_to_idx: unknown maj %d min %d", maj, min);

    return UNKNOWN_BEACON;
}

uint8_t beacon_name_to_idx(char *adv_name)
{
    uint8_t temp_idx = UNKNOWN_BEACON;

    for (int i = 0; i < CONFIG_BLE_DEVICE_COUNT_CONFIGURED; i++){
        if ( (strlen(adv_name) > 0)
            && (strlen(adv_name) == strlen(ble_beacons[i].beacon_data.name))
            && (strncmp((char *) adv_name, (char *) ble_beacons[i].beacon_data.name, strlen(adv_name)) == 0) ){
            temp_idx = i;
            break;
        }
    }

    return temp_idx;
}

__attribute__((unused)) uint8_t num_active_beacon()
{
    uint8_t num_act_beac = 0;
    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            num_act_beac++;
        }
    }
    return num_act_beac;
}

__attribute__((unused)) uint8_t first_active_beacon()
{
    uint8_t first_act_beac = UNKNOWN_BEACON;

    for(int i=0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++){
        if(is_beacon_idx_active(i)){
            first_act_beac = i;
        }
    }

    return first_act_beac;
}

uint8_t get_idx_first_beacon_with_download_status(offline_buffer_status_t status)
{
    uint8_t temp_idx = UNKNOWN_BEACON;

    for (int i = 0; i < CONFIG_BLE_DEVICE_COUNT_CONFIGURED; i++){
        if ( ble_beacons[i].offline_buffer_status == status ){
            temp_idx = i;
            break;
        }
    }

    return temp_idx;
}

bool is_beacon_idx_active(uint16_t idx)
{
    return (s_active_beacon_mask & (1 << idx) );
}

void set_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask |= (1 << idx);
    persist_active_beacon_mask();
}

__attribute__((unused)) void clear_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask &= ~(1 << idx);
    persist_active_beacon_mask();
}

__attribute__((unused)) bool toggle_beacon_idx_active(uint16_t idx)
{
    s_active_beacon_mask ^= (1 << idx);
    persist_active_beacon_mask();

    return s_active_beacon_mask & (1 << idx);
}

void persist_active_beacon_mask()
{
    esp_err_t err;
    UNUSED(err);

    if(CONFIG_ACTIVE_BLE_DEVICE_PERSISTANCE){
        err = save_blemqttproxy_param();
    }
}

beacon_type_t esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len, uint8_t scan_rsp_len)
{
    beacon_type_t result = UNKNOWN_BEACON;

    // ESP_LOG_BUFFER_HEXDUMP(TAG, adv_data, adv_data_len, ESP_LOG_DEBUG);

    if ((adv_data != NULL) && (adv_data_len == 0x1E)){
        if ( (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head_v3, sizeof(mybeacon_common_head_v3)))
            && (!memcmp((uint8_t*)(adv_data + sizeof(mybeacon_common_head_v3)),
                (uint8_t*)&mybeacon_common_vendor_v3, sizeof(mybeacon_common_vendor_v3.proximity_uuid)))){
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v3, true");
            result = BEACON_V3;
        } else {
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v3, false");
            result = UNKNOWN_BEACON;
        }
    } else if ((adv_data != NULL) && (adv_data_len == 0x17)){
        if ( (!memcmp(adv_data, (uint8_t*)&mybeacon_common_head_v4, sizeof(mybeacon_common_head_v4))) ){
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v4, true");

            result = (scan_rsp_len ? BEACON_V4_SR : BEACON_V4);
        } else {
            ESP_LOGD(TAG, "esp_ble_is_mybeacon_packet v4, false");
            result = UNKNOWN_BEACON;
        }
    }

    return result;
}

void decode_mybeacon_packet_v3(esp_ble_mybeacon_v3_t *mybeacon_data,
    uint8_t *idx, uint16_t *maj, uint16_t *min, float *temp, float *humidity, uint16_t *battery,
    int16_t *x, int16_t *y, int16_t *z, int8_t rssi, bool *is_beacon_active, bool *is_beacon_close)
{
    *maj      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.major);
    *min      = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_vendor.minor);
    *idx      = beacon_maj_min_to_idx(*maj, *min);

    if(*idx == UNKNOWN_BEACON){
        return;
    }

    *is_beacon_active = is_beacon_idx_active(*idx);

    if(rssi > CONFIG_PROXIMITY_RSSI_THRESHOLD) {
        ESP_LOGD(TAG, "Close to beacon (0x%04x%04x), idx %d, RSSI %d", *maj, *min, *idx, rssi);
        *is_beacon_close = true;
    }

    if(!*is_beacon_active){
        return;
    }

    *temp       = SHT3_GET_TEMPERATURE_VALUE(
                        LSB_16(mybeacon_data->mybeacon_payload.temp),
                        MSB_16(mybeacon_data->mybeacon_payload.temp) );
    *humidity   = SHT3_GET_HUMIDITY_VALUE(
                        LSB_16(mybeacon_data->mybeacon_payload.humidity),
                        MSB_16(mybeacon_data->mybeacon_payload.humidity) );
    *battery    = ENDIAN_CHANGE_U16(mybeacon_data->mybeacon_payload.battery);
    *x          = (int16_t)(mybeacon_data->mybeacon_payload.x);
    *y          = (int16_t)(mybeacon_data->mybeacon_payload.y);
    *z          = (int16_t)(mybeacon_data->mybeacon_payload.z);
}

void decode_mybeacon_packet_v4(esp_ble_mybeacon_payload_t *mybeacon_payload, uint8_t *ble_adv,
    uint8_t *idx, uint16_t *maj, uint16_t *min, float *temp, float *humidity, uint16_t *battery,
    int16_t *x, int16_t *y, int16_t *z, int8_t rssi, bool *is_beacon_active, bool *is_beacon_close)
{
    *maj        = (uint16_t) ( ((ble_adv[7])<<8) + (ble_adv[8]));
    *min        = (uint16_t) ( ((ble_adv[9])<<8) + (ble_adv[10]));
    *idx        = beacon_maj_min_to_idx(*maj, *min);

    if(*idx == UNKNOWN_BEACON){
        return;
    }

    *is_beacon_active = is_beacon_idx_active(*idx);

    if(rssi > CONFIG_PROXIMITY_RSSI_THRESHOLD) {
        ESP_LOGD(TAG, "Close to beacon (0x%04x%04x), idx %d, RSSI %d", *maj, *min, *idx, rssi);
        *is_beacon_close = true;
    }

    if(!*is_beacon_active){
        return;
    }

    *temp       = SHT3_GET_TEMPERATURE_VALUE(LSB_16(mybeacon_payload->temp), MSB_16(mybeacon_payload->temp) );
    *humidity   = SHT3_GET_HUMIDITY_VALUE(LSB_16(mybeacon_payload->humidity), MSB_16(mybeacon_payload->humidity) );
    *battery    = ENDIAN_CHANGE_U16(mybeacon_payload->battery);
    *x          = (int16_t)(mybeacon_payload->x);
    *y          = (int16_t)(mybeacon_payload->y);
    *z          = (int16_t)(mybeacon_payload->z);
}

void clear_beacon_idx_values(uint16_t idx)
{
    ble_beacons[idx].adv_data.last_seen = 0;
}
