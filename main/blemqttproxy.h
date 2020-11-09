#ifndef __BLEMQTTPROXY_H__
#define __BLEMQTTPROXY_H__

#include <sdkconfig.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs_flash.h"
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_freertos_hooks.h"

#include "esp_system.h"
#include "esp_event.h"
#include "esp_assert.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_flash_partitions.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_netif.h"
#include "errno.h"
#include "esp_wifi.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatt_common_api.h"
#include "esp_gattc_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_bt_defs.h"
#include "esp_ota_ops.h"
#include <esp_http_server.h>
#include "esp_http_client.h"
#include "esp_sntp.h"
#include "mqtt_client.h"

#endif // __BLEMQTTPROXY_H__