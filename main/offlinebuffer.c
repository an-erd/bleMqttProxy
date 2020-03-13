#include "esp_log.h"
#include "offlinebuffer.h"
#include "helperfunctions.h"


static const char *TAG = "offlinebuffer";

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
