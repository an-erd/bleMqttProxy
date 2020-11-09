#include "blemqttproxy.h"

#include "params.h"
#include "beacon.h"

static const char *TAG = "PARAMS";

/* The active (selected) beacons are given in this mask, stored as a param */
param_t blemqttproxy_param = { 0 };

esp_err_t read_blemqttproxy_param()
{
    esp_err_t ret;
    uint16_t mask = 0xFFFF;

    ret = iot_param_load(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param);
    if(ret == ESP_OK){
        ESP_LOGD(TAG, "read_blemqttproxy_param: read param ok, beacon mask %u", blemqttproxy_param.active_beacon_mask);
    } else {
        ESP_LOGW(TAG, "read_blemqttproxy_param: read param failed, ret = %d, initialize and save to NVS", ret);
        blemqttproxy_param.active_beacon_mask = CONFIG_ACTIVE_BLE_DEVICE_MASK;
        ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    }

    mask >>= 16 - CONFIG_BLE_DEVICE_COUNT_USE; // len = 16 bit, only use configured dev to use
    s_active_beacon_mask = blemqttproxy_param.active_beacon_mask & mask;

    ESP_LOGD(TAG, "read_blemqttproxy_param: blemqttproxy_param.active_beacon_mask = %d", s_active_beacon_mask);

    return ret;
}

esp_err_t save_blemqttproxy_param()
{
    esp_err_t ret;
    uint16_t mask = 0xFFFF;
    mask >>= 16 - CONFIG_BLE_DEVICE_COUNT_USE;

    blemqttproxy_param.active_beacon_mask = (blemqttproxy_param.active_beacon_mask & ~mask) | (s_active_beacon_mask & mask);

    ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    if(ret == ESP_OK){
        ESP_LOGD(TAG, "save_blemqttproxy_param: save param ok, beacon mask %u", blemqttproxy_param.active_beacon_mask);
    } else {
        ESP_LOGW(TAG, "save_blemqttproxy_param: save param failed, ret = %d, initialize and save to NVS", ret);
        blemqttproxy_param.active_beacon_mask = s_active_beacon_mask;
        ret = iot_param_save(PARAM_NAMESPACE, PARAM_KEY, &blemqttproxy_param, sizeof(param_t));
    }

    s_active_beacon_mask = blemqttproxy_param.active_beacon_mask & mask;

    ESP_LOGD(TAG, "save_blemqttproxy_param: blemqttproxy_param.active_beacon_mask = %d", s_active_beacon_mask);

    return ret;
}
