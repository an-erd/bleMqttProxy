#include "esp_log.h"
#include "offlinebuffer.h"
#include "helperfunctions.h"


static const char *TAG = "offlinebuffer";

EventGroupHandle_t offlinebuffer_evg;
char buffer_download_device_name[20];    // max len should be 6 chars
ble_os_meas_t buffer_download[1250];
uint16_t buffer_download_count = 0;
bool buffer_download_csv_data_available = false;    // data is available and ready to send back

void offline_buffer_clear()
{
    buffer_download_count = 0;
    strcpy(buffer_download_device_name, "");
    buffer_download_csv_data_available = false;
}

void offline_buffer_csv_prepare()
{
    ESP_LOGI(TAG, "offline_buffer_csv_prepare >");

    for (uint16_t i = 0; i < buffer_download_count; i++){
        buffer_download[i].csv_date_time    = buffer_download[i].time_stamp / 86400. + 25569;
        buffer_download[i].temperature_f
            = SHT3_GET_TEMPERATURE_VALUE(MSB_16(buffer_download[i].temperature), LSB_16(buffer_download[i].temperature));
        buffer_download[i].humidity_f
            = SHT3_GET_HUMIDITY_VALUE(MSB_16(buffer_download[i].humidity), LSB_16(buffer_download[i].humidity));
    }


    ESP_LOGI(TAG, "offline_buffer_csv_prepare <");
}

void offlinebuffer_task(void* pvParameters)
{
    typedef enum
    {
        BLE_READ,
        CSV_PREPARE,
        READY_WEB_CSV_SEND,
        NO_STEP,
    } offline_buffer_steps_t;
    offline_buffer_steps_t current_step = NO_STEP;

    // static httpd_handle_t server = NULL;
    EventBits_t uxBits;

    while (1)
    {
        uxBits = xEventGroupWaitBits(offlinebuffer_evg,
            OFFLINE_BUFFER_BLE_READ_EVT | OFFLINE_BUFFER_CSV_PREPARE_EVT | OFFLINE_BUFFER_READY_CSV_SEND_EVT | OFFLINE_BUFFER_RESET_EVT,
            pdTRUE, pdFALSE, portMAX_DELAY);

        if (uxBits & OFFLINE_BUFFER_BLE_READ_EVT) {
            ESP_LOGI(TAG, "offlinebuffer_task OFFLINE_BUFFER_BLE_READ_EVT device=%s", buffer_download_device_name);
            current_step = BLE_READ;
            xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_TAKE_NEXT_AVD_EVT);

        } else if (uxBits & OFFLINE_BUFFER_CSV_PREPARE_EVT) {
            ESP_LOGI(TAG, "offlinebuffer_task OFFLINE_BUFFER_CSV_PREPARE_EVT");
            current_step = CSV_PREPARE;
            offline_buffer_csv_prepare();
            xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_READY_CSV_SEND_EVT);

        } else if (uxBits & OFFLINE_BUFFER_READY_CSV_SEND_EVT) {
            ESP_LOGI(TAG, "offlinebuffer_task OFFLINE_BUFFER_READY_CSV_SEND_EVT");
            current_step = READY_WEB_CSV_SEND;
            buffer_download_csv_data_available = true;

        } else if (uxBits & OFFLINE_BUFFER_RESET_EVT) {
            ESP_LOGI(TAG, "offlinebuffer_task OFFLINE_BUFFER_RESET_EVT");
            current_step = NO_STEP;
            xEventGroupClearBits(offlinebuffer_evg,
                OFFLINE_BUFFER_BLE_READ_EVT | OFFLINE_BUFFER_TAKE_NEXT_AVD_EVT | OFFLINE_BUFFER_CSV_PREPARE_EVT | OFFLINE_BUFFER_READY_CSV_SEND_EVT | OFFLINE_BUFFER_RESET_EVT );
            offline_buffer_clear();
        }
    }

    vTaskDelete(NULL);
}
