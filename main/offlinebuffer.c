#include "esp_log.h"
#include "offlinebuffer.h"
#include "helperfunctions.h"


static const char *TAG = "offlinebuffer";

EventGroupHandle_t offlinebuffer_evg;

char *offline_buffer_status_to_str(offline_buffer_status_t status)
{
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


void offline_buffer_clear()
{
    // TODO
}


void offlinebuffer_task(void* pvParameters)
{
    // static httpd_handle_t server = NULL;
    EventBits_t uxBits;

    while (1)
    {
        uxBits = xEventGroupWaitBits(offlinebuffer_evg,
            OFFLINE_BUFFER_BLE_READ_EVT | OFFLINE_BUFFER_READY_EVT | OFFLINE_BUFFER_RESET_EVT,
            pdTRUE, pdFALSE, portMAX_DELAY);

        if (uxBits & OFFLINE_BUFFER_BLE_READ_EVT) {
            ESP_LOGD(TAG, "offlinebuffer_task OFFLINE_BUFFER_BLE_READ_EVT");
            xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_TAKE_NEXT_AVD_EVT);
        } else if (uxBits & OFFLINE_BUFFER_READY_EVT) {
            ESP_LOGD(TAG, "offlinebuffer_task OFFLINE_BUFFER_READY_EVT");
        } else if (uxBits & OFFLINE_BUFFER_RESET_EVT) {
            ESP_LOGD(TAG, "offlinebuffer_task OFFLINE_BUFFER_RESET_EVT");
            xEventGroupClearBits(offlinebuffer_evg,
                OFFLINE_BUFFER_BLE_READ_EVT | OFFLINE_BUFFER_TAKE_NEXT_AVD_EVT | OFFLINE_BUFFER_READY_EVT | OFFLINE_BUFFER_RESET_EVT );
            offline_buffer_clear();
        }
    }

    vTaskDelete(NULL);
}
