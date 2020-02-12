#ifndef __LOCALSENSOR_H__
#define __LOCALSENSOR_H__

// Local sensor: DS18B20 temperatur sensor

#include <stdint.h>
#include <string.h>
#include <stdbool.h>


// #if CONFIG_LOCAL_SENSORS_TEMPERATURE==1

#include "owb.h"
#include "owb_rmt.h"
#include "ds18b20.h"

#define GPIO_DS18B20_0                      (CONFIG_ONE_WIRE_GPIO)
#define OWB_MAX_DEVICES                     (CONFIG_OWB_MAX_DEVICES)
#define DS18B20_RESOLUTION                  (DS18B20_RESOLUTION_12_BIT)
#define LOCAL_SENSOR_SAMPLE_PERIOD          (1000)   // milliseconds

typedef struct  {
    float   temperature;
    int64_t last_seen;
} local_temperature_data_t;

extern local_temperature_data_t local_temperature_data[OWB_MAX_DEVICES];

void init_owb_tempsensor();
void localsensor_task(void* pvParameters);

// #endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
#endif // __LOCALSENSOR_H__
