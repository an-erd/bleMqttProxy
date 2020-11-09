#ifndef __PARAMS_H__
#define __PARAMS_H__

#include "esp_err.h"
#include "iot_param.h"

/* IOT param */
#define PARAM_NAMESPACE "blemqttproxy"
#define PARAM_KEY       "activebeac"

typedef struct {
    uint16_t active_beacon_mask;
} param_t;

extern param_t blemqttproxy_param;

esp_err_t read_blemqttproxy_param();
esp_err_t save_blemqttproxy_param();

#endif // __PARAMS_H__