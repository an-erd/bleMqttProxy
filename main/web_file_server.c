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
#include "helperfunctions.h"

static const char *TAG = "web_file_server";
static const char *web_file_server_commands[WEBFILESERVER_NUM_ENTRIES] = {
    "stat",
    "req",
    "dl",
    "reset",
    "list",
};


static esp_err_t http_resp_csv_download(httpd_req_t *req, uint8_t idx)
{
    char buffer[128];
    uint16_t count = ble_beacons[idx].offline_buffer_count;

    httpd_resp_set_type(req, "application/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=example.csv");
    httpd_resp_sendstr_chunk(req, ble_beacons[idx].beacon_data.name);
    httpd_resp_sendstr_chunk(req, "\n");
    httpd_resp_sendstr_chunk(req, "Seq.nr.;EPOCH Time;Time-Date;Temperatur;Humidity \n");

    for (uint16_t i = 0; i < count; i++){
        snprintf(buffer, 128, "%6d;%10d;%10.5f;% 2.1f;%3.1f\n",
            ble_beacons[idx].p_buffer_download[i].sequence_number,
            ble_beacons[idx].p_buffer_download[i].time_stamp,
            ble_beacons[idx].p_buffer_download[i].csv_date_time,
            ble_beacons[idx].p_buffer_download[i].temperature_f,
            ble_beacons[idx].p_buffer_download[i].humidity_f);
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
    uint8_t h, m, s;
    uint16_t last_seen_sec_gone;
    char buffer[128];
    uint8_t num_devices = CONFIG_BLE_DEVICE_COUNT_CONFIGURED;
    offline_buffer_status_t status;


    ESP_LOGD(TAG, "http_resp_list_devices, count = %d", num_devices);

    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><body style=\"font-family:Arial\">\n");
    httpd_resp_sendstr_chunk(req, "<h1 style=\"text-align: center;\">Beacon List</h1>\n");
    httpd_resp_sendstr_chunk(req, "<table style=\"margin-left: auto; margin-right: auto;\" border=\"0\" width=\"600\" bgcolor=\"#e0e0e0\">\n");
    httpd_resp_sendstr_chunk(req, "<tbody>\n");
    httpd_resp_sendstr_chunk(req, "<tr bgcolor=\"#c0c0c0\">\n");
    httpd_resp_sendstr_chunk(req, "<td style=\"text-align: center;\">Name</td>\n");
    httpd_resp_sendstr_chunk(req, "<td style=\"text-align: center;\">Last seen ago</td>\n");
    httpd_resp_sendstr_chunk(req, "<td style=\"text-align: center;\">Status</td>\n");
    httpd_resp_sendstr_chunk(req, "<td style=\"text-align: center;\">Command</td>\n");
    httpd_resp_sendstr_chunk(req, "<td style=\"text-align: center;\">Download file</td>\n");
    httpd_resp_sendstr_chunk(req, "</tr>\n");

    for (int i = 0; i < num_devices; i++){

        // Name
        httpd_resp_sendstr_chunk(req, "<tr>\n<td>\n");
        httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
        httpd_resp_sendstr_chunk(req, "</td>\n");

        // Last seen
        if(is_beacon_idx_active(i)){
            httpd_resp_sendstr_chunk(req, "<td>");
            if(ble_beacons[i].adv_data.last_seen == 0){
                httpd_resp_sendstr_chunk(req, "never");
            } else {
                last_seen_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.last_seen) / 1000000;
                convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                snprintf(buffer, 128, "%02d:%02d:%02d", h, m, s);
                httpd_resp_sendstr_chunk(req, buffer);
            }
            httpd_resp_sendstr_chunk(req, "</td>\n<td>");

            // Download Status
            status = ble_beacons[i].offline_buffer_status;
            httpd_resp_sendstr_chunk(req, offline_buffer_descr_status_to_str(status));
            httpd_resp_sendstr_chunk(req, "</td>\n");

            // Command
            switch (status){
                case OFFLINE_BUFFER_STATUS_NONE:
                    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?beac=");
                    httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
                    httpd_resp_sendstr_chunk(req, "&amp;cmd=req\">request</a></td>\n");
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
                    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?beac=");
                    httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
                    httpd_resp_sendstr_chunk(req, "&amp;cmd=rst\">cancel</a></td>\n");
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
                    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?beac=");
                    httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
                    httpd_resp_sendstr_chunk(req, "&amp;cmd=rst\">cancel</a></td>\n");
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
                    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?beac=");
                    httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
                    httpd_resp_sendstr_chunk(req, "&amp;cmd=rst\">Clear</a></td>\n");
                    break;
                case OFFLINE_BUFFER_STATUS_UNKNOWN:
                    httpd_resp_sendstr_chunk(req, "<td>none</td>\n");
                    break;
            }

            // Download
            switch (status){
                case OFFLINE_BUFFER_STATUS_NONE:
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
                    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?beac=");
                    httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
                    httpd_resp_sendstr_chunk(req, "&amp;cmd=dl\">Download</a></td>\n");
                    break;
                case OFFLINE_BUFFER_STATUS_UNKNOWN:
                    break;
            }

            httpd_resp_sendstr_chunk(req, "</tr>\n");
        } else {
            httpd_resp_sendstr_chunk(req, "<td>");
            httpd_resp_sendstr_chunk(req, "inactive");
            httpd_resp_sendstr_chunk(req, "</td>\n");
        }
    }
    httpd_resp_sendstr_chunk(req, "</tbody></table>");
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
    uint8_t idx = UNKNOWN_BEACON;
    offline_buffer_status_t status;
    web_file_server_cmd_t cmd = WEBFILESERVER_NO_CMD;

    ESP_LOGD(TAG, "csv_get_handler >");

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            ESP_LOGD(TAG, "    URL query=%s", buf);

            // get device_name out of query
            ret = httpd_query_key_value(buf, "beac", device_name, sizeof(device_name));
            switch (ret){
                case ESP_OK:
                    ESP_LOGD(TAG, "    beac=%s", device_name);
                    device_name_set = true;
                    break;
                case ESP_ERR_HTTPD_RESULT_TRUNC:
                    ESP_LOGD(TAG, "    beac=%s, truncated!", device_name);
                    device_name_set = true;
                    break;
                default:
                    ESP_LOGD(TAG, "    beac=not set");
                    break;
            }

            // get command out of query, i.e. stat, prep, dl, reset
            ret = httpd_query_key_value(buf, "cmd", param, sizeof(param));
            switch (ret){
                case ESP_OK:
                    ESP_LOGD(TAG, "    cmd=%s", param);
                    int cmd_len = strlen(param);
                    for (int i = 0; i < WEBFILESERVER_NUM_ENTRIES; i++) {
                        if ( (cmd_len == strlen(web_file_server_commands[i])) && (strncmp((char *) param, web_file_server_commands[i], cmd_len) == 0) ) {
                            cmd = (web_file_server_cmd_t) i;
                            break;
                        }
                    }
                    break;
                default:
                    ESP_LOGD(TAG, "    cmd=not set");
                    break;
            }
        }
        free(buf);
    }

    if (device_name_set){
        idx = beacon_name_to_idx(device_name);
    }

    bool is_available = (idx != UNKNOWN_BEACON)
        && (ble_beacons[idx].offline_buffer_status == OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE);

    switch (cmd) {
        case WEBFILESERVER_CMD_STAT:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_STAT");


            snprintf(resp_str, 128, "Status requested for device %s, device known %s, available %s, offline_buffer_status = %s",
                device_name_set ? device_name : "n/a",
                idx != UNKNOWN_BEACON ? "y" : "n",
                is_available ? "y" : "n",
                offline_buffer_status_to_str(ble_beacons[idx].offline_buffer_status) );

            httpd_resp_send(req, resp_str, strlen(resp_str));
            break;

        case WEBFILESERVER_CMD_REQ:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_REQ");
            ESP_LOGD(TAG, "Data requested for device %s, idx %d",  device_name_set ? device_name : "n/a", idx );
            if( idx == UNKNOWN_BEACON ){
                ESP_LOGD(TAG, "unknown device");
            } else {
                switch(ble_beacons[idx].offline_buffer_status){
                    case OFFLINE_BUFFER_STATUS_NONE:
                        ble_beacons[idx].p_buffer_download = (ble_os_meas_t *) malloc(sizeof(ble_os_meas_t) * CONFIG_OFFLINE_BUFFER_SIZE);
                        ble_beacons[idx].offline_buffer_count = 0;
                        ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED;
                        break;
                    case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
                        ESP_LOGD(TAG, "already requested");
                        break;
                    case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
                        ESP_LOGD(TAG, "already in progress");
                        break;
                    case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
                        ESP_LOGD(TAG, "already available for download");
                        break;
                    default:
                        ESP_LOGD(TAG, "unhandled switch case");
                        break;
                }
            }
            xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_BLE_READ_EVT);
            break;

        case WEBFILESERVER_CMD_DL:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_DL");
            if (!is_available){
                snprintf(resp_str, 128, "Download requested for device %s, not available",  device_name_set?device_name:"n/a");
                httpd_resp_send(req, resp_str, strlen(resp_str));
            } else {
                http_resp_csv_download(req, idx);
                xEventGroupSetBits(offlinebuffer_evg, OFFLINE_BUFFER_RESET_EVT);
            }
            break;

        case WEBFILESERVER_CMD_RESET:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_RESET");
            status = ble_beacons[idx].offline_buffer_status;
            switch(status){
                case OFFLINE_BUFFER_STATUS_NONE:
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
                    ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_NONE;
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
                    ESP_LOGD(TAG, "already in progress, wait for now");
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
                    ESP_LOGD(TAG, "already available for download");
                    break;
                default:
                    ESP_LOGD(TAG, "unhandled switch case");
                    break;
            }
            http_resp_list_devices(req);
            break;

        case WEBFILESERVER_CMD_LIST:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_LIST");
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

    ESP_LOGE(TAG, "Error starting server!");
    return NULL;
}

void stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    httpd_stop(server);
}
