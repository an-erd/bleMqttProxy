#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"

#include "esp_http_server.h"
#include "web_file_server.h"
#include "beacon.h"
#include "offlinebuffer.h"

static const char *TAG = "web_file_server";
static const char *web_file_server_commands[WEBFILESERVER_NUM_ENTRIES] = {
    "stat",
    "prep",
    "dl",
    "reset",
    "list",
};

static esp_err_t http_resp_csv_download(httpd_req_t *req, uint8_t idx)
{
    char buffer[128];

    httpd_resp_set_type(req, "application/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=example.csv");
    httpd_resp_sendstr_chunk(req, get_known_beacon_name(idx));
    httpd_resp_sendstr_chunk(req, "\n");
    httpd_resp_sendstr_chunk(req, "Seq.nr.;EPOCH Time;Time-Date;Temperatur;Humidity \n");

    for (uint16_t i = 0; i < buffer_download_count; i++){
        snprintf(buffer, 128, "%6d;%10d;%10.3f;% 2.1f;%3.1f\n",
            buffer_download[i].sequence_number,
            buffer_download[i].time_stamp,
            buffer_download[i].csv_date_time,
            buffer_download[i].temperature_f,
            buffer_download[i].humidity_f);
        for (uint8_t j = 0; j < 128; j++){
            if (buffer[j] == '.')
                buffer[j] = ',';
            if (buffer[j] == 0)
                break;
        }
        httpd_resp_sendstr_chunk(req, buffer);
    }
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}
static esp_err_t http_resp_list_devices(httpd_req_t *req)
{
    uint8_t num_devices = num_beacon_name_known();
    ESP_LOGI(TAG, "http_resp_list_devices, count = %d", num_devices);
    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body>");
    for (int i = 0; i < num_devices; i++){
        httpd_resp_sendstr_chunk(req, get_known_beacon_name(i));
        httpd_resp_sendstr_chunk(req, "\n");
    }
    httpd_resp_sendstr_chunk(req, "</body></html>");
    httpd_resp_sendstr_chunk(req, NULL);

    return ESP_OK;
}

esp_err_t csv_get_handler(httpd_req_t *req)
{
    esp_err_t ret;

    char*  buf;
    size_t buf_len;
    bool device_name_set = false;
    char device_name[32];
    char param[32];
    char resp_str[128];

    web_file_server_cmd_t cmd = WEBFILESERVER_NO_CMD;

    ESP_LOGI(TAG, "csv_get_handler >");

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGI(TAG, "    URL query=%s", buf);

            // get device_name out of query
            ret = httpd_query_key_value(buf, "beac", device_name, sizeof(device_name));
            switch (ret){
                case ESP_OK:
                    ESP_LOGI(TAG, "    beac=%s", device_name);
                    device_name_set = true;
                    break;
                case ESP_ERR_HTTPD_RESULT_TRUNC:
                    ESP_LOGI(TAG, "    beac=%s, truncated!", device_name);
                    device_name_set = true;
                    break;
                default:
                    ESP_LOGI(TAG, "    beac=not set");
                    break;
            }

            // get command out of query, i.e. stat, prep, dl, reset
            ret = httpd_query_key_value(buf, "cmd", param, sizeof(param));
            switch (ret){
                case ESP_OK:
                    ESP_LOGI(TAG, "    cmd=%s", param);
                    int cmd_len = strlen(param);
                    for (int i = 0; i < WEBFILESERVER_NUM_ENTRIES; i++) {
                        if ( (cmd_len == strlen(web_file_server_commands[i])) && (strncmp((char *) param, web_file_server_commands[i], cmd_len) == 0) ) {
                            cmd = (web_file_server_cmd_t) i;
                            break;
                        }
                    }
                    break;
                default:
                    ESP_LOGI(TAG, "    cmd=not set");
                    break;
            }
        }
        free(buf);
    }

    bool is_available = buffer_download_csv_data_available && device_name_set
        && (strlen(buffer_download_device_name) == strlen(device_name))
        && (strncmp((char *) buffer_download_device_name, device_name, strlen(device_name)) == 0);

    switch (cmd) {
        case WEBFILESERVER_CMD_STAT:
            ESP_LOGI(TAG, "csv_get_handler WEBFILESERVER_CMD_STAT");
            snprintf(resp_str, 128, "Status requested for device %s, available %s",  device_name_set?device_name:"n/a", is_available?"y":"n");
            httpd_resp_send(req, resp_str, strlen(resp_str));
            break;
        case WEBFILESERVER_CMD_PREP:
            ESP_LOGI(TAG, "csv_get_handler WEBFILESERVER_CMD_PREP");
            snprintf(resp_str, 128, "Prepare requested for device %s",  device_name_set?device_name:"n/a");
            strncpy(buffer_download_device_name, device_name, 20);
            xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_BLE_READ_EVT);
            httpd_resp_send(req, resp_str, strlen(resp_str));
            break;
        case WEBFILESERVER_CMD_DL:
            ESP_LOGI(TAG, "csv_get_handler WEBFILESERVER_CMD_DL");
            if (!is_available){
                snprintf(resp_str, 128, "Download requested for device %s, not available",  device_name_set?device_name:"n/a");
                httpd_resp_send(req, resp_str, strlen(resp_str));
            } else {
                http_resp_csv_download(req, is_beacon_name_known(device_name));
                xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_RESET_EVT);
            }

            break;
        case WEBFILESERVER_CMD_RESET:
            ESP_LOGI(TAG, "csv_get_handler WEBFILESERVER_CMD_RESET");
            break;
        case WEBFILESERVER_CMD_LIST:
            ESP_LOGI(TAG, "csv_get_handler WEBFILESERVER_CMD_LIST");
            http_resp_list_devices(req);
            break;
        default:
            break;
    }

    return ESP_OK;
}

httpd_uri_t csv = {
    .uri       = "/csv",
    .method    = HTTP_GET,
    .handler   = csv_get_handler,
    .user_ctx  = NULL
};

httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &csv);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}
