#ifndef __OFFLINE_BUFFER_H__
#define __OFFLINE_BUFFER_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_http_server.h"

typedef enum
{
    OFFLINE_BUFFER_STATUS_NONE = 0,                             /**< Not requested, not available */
    OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED,
    OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS,
    OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE,
    OFFLINE_BUFFER_STATUS_UNKNOWN = 99,
} offline_buffer_status_t;

typedef struct
{
    uint16_t        sequence_number;                            /**< Sequence number */
    uint32_t        time_stamp;                                 /**< EPOCH Time stamp */

    float           csv_date_time;                              /**< = EPOCH time / 86400. + 25569 */
    float           temperature_f;
    float           humidity_f;
} __attribute__ ((packed)) ble_os_meas_t;

char * offline_buffer_status_to_str(offline_buffer_status_t status);
char * offline_buffer_descr_status_to_str(offline_buffer_status_t status);

#endif // __OFFLINE_BUFFER_H__