#ifndef __OFFLINE_BUFFER_H__
#define __OFFLINE_BUFFER_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

#define OFFLINE_BUFFER_BLE_READ_EVT        (BIT0)
#define OFFLINE_BUFFER_CSV_PREPARE_EVT     (BIT1)
#define OFFLINE_BUFFER_READY_CSV_SEND_EVT  (BIT2)
#define OFFLINE_BUFFER_RESET_EVT           (BIT3)
#define OFFLINE_BUFFER_TAKE_NEXT_AVD_EVT   (BIT4)               /**< Take next appropriate BLE advertisement to connect */
extern EventGroupHandle_t offlinebuffer_evg;

typedef struct
{
    uint16_t        sequence_number;                            /**< Sequence number */
    uint32_t        time_stamp;                                 /**< Time stamp */
    uint16_t        temperature;                                /**< Sensor temperature value */
    uint16_t        humidity;                                   /**< Sensor humidity value */
    // ---- following fileds for csv output
    float           csv_date_time;                              /**< = EPOCH time / 86400. + 25569 */
    float           temperature_f;
    float           humidity_f;
} __attribute__ ((packed)) ble_os_meas_t;


extern char buffer_download_device_name[];
extern ble_os_meas_t buffer_download[];
extern uint16_t buffer_download_count;
extern bool buffer_download_csv_data_available;

typedef struct
{
    httpd_handle_t server;
    char device_name[20];
} offline_buffer_params_t;

void offline_buffer_clear();
void offlinebuffer_task(void* pvParameters);

#endif // __OFFLINE_BUFFER_H__