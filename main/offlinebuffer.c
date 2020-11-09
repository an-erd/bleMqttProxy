#include "blemqttproxy.h"

#include "offlinebuffer.h"
#include "helperfunctions.h"
#include "beacon.h"

static const char* TAG = "OFFLINEBUFFER";

EventGroupHandle_t offlinebuffer_evg;

char *offline_buffer_status_to_str(offline_buffer_status_t status)
{
    UNUSED(TAG);
    char *status_str = NULL;
    switch(status) {
        case OFFLINE_BUFFER_STATUS_NONE:
            status_str = "OFFLINE_BUFFER_STATUS_NONE";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
            status_str = "OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
            status_str = "OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
            status_str = "OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE";
            break;
        case OFFLINE_BUFFER_STATUS_UNKNOWN:
            status_str = "OFFLINE_BUFFER_STATUS_UNKNOWN";
            break;
        default:
            status_str = "INVALID OFFLINE_BUFFER_STATUS";
            break;
    }

   return status_str;
}

char *offline_buffer_descr_status_to_str(offline_buffer_status_t status)
{
    char *status_str = NULL;
    switch(status) {
        case OFFLINE_BUFFER_STATUS_NONE:
            status_str = "None";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
            status_str = "Download Requested";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
            status_str = "Download in progress";
            break;
        case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
            status_str = "Download File Available";
            break;
        case OFFLINE_BUFFER_STATUS_UNKNOWN:
            status_str = "Unknown";
            break;
        default:
            status_str = "Invalid";
            break;
    }

    return status_str;
}

void alloc_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ESP_LOGD(TAG, "alloc_offline_buffer free heap() %d, needed %d", esp_get_free_heap_size(), sizeof(ble_os_meas_t) * CONFIG_OFFLINE_BUFFER_SIZE);
    ble_beacons[idx].p_buffer_download = (ble_os_meas_t *) malloc(sizeof(ble_os_meas_t) * CONFIG_OFFLINE_BUFFER_SIZE);
    ESP_LOGD(TAG, "alloc_offline_buffer p_buffer_download %d ", ble_beacons[idx].p_buffer_download != NULL);
    ble_beacons[idx].offline_buffer_count = 0;
    ble_beacons[idx].offline_buffer_status = status;
}

void free_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ble_beacons[idx].offline_buffer_status = status;
    ble_beacons[idx].offline_buffer_count = 0;
    free(ble_beacons[idx].p_buffer_download);
}

void reset_offline_buffer(uint8_t idx, offline_buffer_status_t status)
{
    ble_beacons[idx].offline_buffer_status = status;
    ble_beacons[idx].offline_buffer_count = 0;
}
