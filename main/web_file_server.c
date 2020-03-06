#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_defs.h"

#include "web_file_server.h"
#include "beacon.h"
#include "ble.h"
#include "offlinebuffer.h"
#include "helperfunctions.h"
#include "ota.h"

static const char *TAG = "web_file_server";
static const char *web_file_server_commands[WEBFILESERVER_NUM_ENTRIES] = {
    "stat",
    "req",
    "dl",
    "rst",
    "list",
    "ota",
    "reboot"
};

static esp_err_t redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/csv?cmd=list");
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t print_bond_devices(httpd_req_t *req)
{
    char buffer[128];
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    snprintf(buffer, 128, "<tr>\n<td valign=top>Bond device num</td><td> %d </td></tr>\n", dev_num);
    httpd_resp_sendstr_chunk(req, buffer);

    for (int i = 0; i < dev_num; i++) {
        snprintf(buffer, 128, "<tr>\n<td valign=top>Bond device %d</td><td style=\"white-space:nowrap;\">ADDR: ", i);
        httpd_resp_sendstr_chunk(req, buffer);
        for (int v = 0; v < sizeof(esp_bd_addr_t); v++){
            snprintf(buffer, 128, "%02X ", dev_list[i].bd_addr[v]);
            httpd_resp_sendstr_chunk(req, buffer);
        }
        httpd_resp_sendstr_chunk(req, "<br>IRK: ");
        for (int v = 0; v < sizeof(esp_bt_octet16_t); v++){
            snprintf(buffer, 128, "%02X ", dev_list[i].bond_key.pid_key.irk[v]);
            httpd_resp_sendstr_chunk(req, buffer);
        }

        httpd_resp_sendstr_chunk(req, "");
    }

    free(dev_list);

    return ESP_OK;
}

static esp_err_t http_resp_csv_download(httpd_req_t *req, uint8_t idx)
{
    char buffer[128], buffer2[32];
    struct tm ts;
    uint16_t count = ble_beacons[idx].offline_buffer_count;

    httpd_resp_set_type(req, "application/csv");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=example.csv");
    httpd_resp_sendstr_chunk(req, ble_beacons[idx].beacon_data.name);
    httpd_resp_sendstr_chunk(req, "\n");
    httpd_resp_sendstr_chunk(req, "Seq.nr.;EPOCH Time;Time-Date;Temperature;Humidity\n");

    for (uint16_t i = 0; i < count; i++){
        // memcpy(&epoch_time, localtime((time_t*)&ble_beacons[idx].p_buffer_download[i].time_stamp, sizeof (struct tm));
        ts = *localtime((time_t*)&ble_beacons[idx].p_buffer_download[i].time_stamp);
        strftime(buffer2, sizeof(buffer2), "%d.%m.%y %H:%M:%S", &ts);
        snprintf(buffer, 128, "%6d;%10d;%s;% 2.1f;%3.1f\n",
            ble_beacons[idx].p_buffer_download[i].sequence_number,
            ble_beacons[idx].p_buffer_download[i].time_stamp,
            buffer2,
            ble_beacons[idx].p_buffer_download[i].temperature_f,
            ble_beacons[idx].p_buffer_download[i].humidity_f);
        for (uint8_t j = 36; j < 128; j++){
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
    esp_err_t ret;
    uint8_t h, m, s;
    uint16_t d;
    uint16_t last_seen_sec_gone, last_send_sec_gone;
    uint32_t uptime_sec = esp_timer_get_time() / 1000000;
    char buffer[128];
    uint8_t num_devices = CONFIG_BLE_DEVICE_COUNT_CONFIGURED;
    offline_buffer_status_t status;
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();

    httpd_resp_sendstr_chunk(req, "<!DOCTYPE html><html><head>\n");
    httpd_resp_sendstr_chunk(req, "<title>Beacon List</title>\n");
    httpd_resp_sendstr_chunk(req, "<meta http-equiv=\"refresh\" content=\"5\">\n");
    httpd_resp_sendstr_chunk(req, "<body style=\"font-family:Arial\">\n");
    httpd_resp_sendstr_chunk(req, "<h1 style=\"text-align: center;\">Beacon List</h1>\n");

    // Beacon list table
    httpd_resp_sendstr_chunk(req, "<table style=\"margin-left: auto; margin-right: auto;\" border=\"0\" width=\"600\" bgcolor=\"#e0e0e0\">\n");
    httpd_resp_sendstr_chunk(req, "<tbody>\n");
    httpd_resp_sendstr_chunk(req, "<tr bgcolor=\"#c0c0c0\">\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Name</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Address</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Last seen<br>(sec ago)</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">MQTT send<br>(sec ago)</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Status</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Command</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Download file</th>\n");
    httpd_resp_sendstr_chunk(req, "</tr>\n");

    for (int i = 0; i < num_devices; i++){

        // Name
        httpd_resp_sendstr_chunk(req, "<tr>\n<td>\n");
        httpd_resp_sendstr_chunk(req, ble_beacons[i].beacon_data.name);
        httpd_resp_sendstr_chunk(req, "</td>\n");

        // bd_addr
        httpd_resp_sendstr_chunk(req, "<td style=\"white-space:nowrap;\">\n");
        if(ble_beacons[i].beacon_data.bd_addr_set){
            for (int v = 0; v < sizeof(esp_bd_addr_t); v++){
                snprintf(buffer, 128, "%02X ", ble_beacons[i].beacon_data.bd_addr[v]);
                httpd_resp_sendstr_chunk(req, buffer);
            }
        }
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

            if(ble_beacons[i].adv_data.mqtt_last_send == 0){
                httpd_resp_sendstr_chunk(req, "never");
            } else {
                last_send_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.mqtt_last_send) / 1000000;
                convert_s_hhmmss(last_send_sec_gone, &h, &m, &s);
                snprintf(buffer, 128, "%02d:%02d:%02d", h, m, s);
                httpd_resp_sendstr_chunk(req, buffer);
            }
            httpd_resp_sendstr_chunk(req, "</td>\n<td style=\"white-space:nowrap;\">");

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

        } else {
            httpd_resp_sendstr_chunk(req, "<td>");
            httpd_resp_sendstr_chunk(req, "inactive");
            httpd_resp_sendstr_chunk(req, "</td>\n");
        }
        httpd_resp_sendstr_chunk(req, "</tr>\n");
    }

    // Reboot and OTA commands for device
    httpd_resp_sendstr_chunk(req, "<tr>\n<td>Device</td><td></td><td></td><td></td>");
    httpd_resp_sendstr_chunk(req, "<td><a href=\"/csv?cmd=reboot\">Reboot</a></td><td></td></tr>\n");
    httpd_resp_sendstr_chunk(req, "<tr>\n<td>Device</td><td></td><td></td><td></td>");
    httpd_resp_sendstr_chunk(req, "<td style=\"white-space:nowrap;\"><a href=\"/csv?cmd=ota\">Start OTA</a></td><td></td></tr>\n");
    httpd_resp_sendstr_chunk(req, "</tbody></table>");

    httpd_resp_sendstr_chunk(req, "</br></br>\n");

    // Status table
    httpd_resp_sendstr_chunk(req, "<table style=\"margin-left: auto; margin-right: auto;\" border=\"0\" width=\"600\" bgcolor=\"#e0e0e0\">\n");
    httpd_resp_sendstr_chunk(req, "<tbody>\n");

    httpd_resp_sendstr_chunk(req, "<tr bgcolor=\"#c0c0c0\">\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Flag/Field</th>\n");
    httpd_resp_sendstr_chunk(req, "<th style=\"text-align: center;\">Status</th>\n");
    httpd_resp_sendstr_chunk(req, "</tr>\n");

    convert_s_ddhhmmss(uptime_sec, &d, &h, &m, &s);
    snprintf(buffer, 128, "%3dd %02d:%02d:%02d", d, h, m, s);
    httpd_resp_sendstr_chunk(req, "<tr>\n<td>uptime</td><td>");
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_connect</td><td>");
    httpd_resp_sendstr_chunk(req, (gattc_connect == true ? "true":"false"));
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_is_connected</td><td>");
    httpd_resp_sendstr_chunk(req, (gattc_is_connected == true ? "true":"false"));
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_scanning</td><td>");
    httpd_resp_sendstr_chunk(req, (gattc_scanning == true ? "true":"false"));
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_offline_buffer_downloading</td><td>");
    httpd_resp_sendstr_chunk(req, (gattc_offline_buffer_downloading == true ? "true":"false"));
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_give_up_now</td><td>");
    httpd_resp_sendstr_chunk(req, (gattc_give_up_now == true ? "true":"false"));
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>gattc_connect_beacon_idx</td><td>");
    snprintf(buffer, 128, "%d", gattc_connect_beacon_idx);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>app_desc->version</td><td>");
    snprintf(buffer, 128, "%s", app_desc->version);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>app_desc->project_name</td><td>");
    snprintf(buffer, 128, "%s", app_desc->project_name);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    httpd_resp_sendstr_chunk(req, "<tr>\n<td>app_desc->idf_ver</td><td>");
    snprintf(buffer, 128, "%s", app_desc->idf_ver);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    const esp_partition_t *running = esp_ota_get_running_partition();
    httpd_resp_sendstr_chunk(req, "<tr>\n<td>OTA running partition</td><td>");
    snprintf(buffer, 128, "type %d subtype %d (offset 0x%08x)", running->type, running->subtype, running->address);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    const esp_partition_t *configured = esp_ota_get_boot_partition();
    httpd_resp_sendstr_chunk(req, "<tr>\n<td>OTA configured partition</td><td>");
    snprintf(buffer, 128, "type %d subtype %d (offset 0x%08x)", configured->type, configured->subtype, configured->address);
    httpd_resp_sendstr_chunk(req, buffer);
    httpd_resp_sendstr_chunk(req, "</td></tr>\n");

    // print bond devices
    ret = print_bond_devices(req);
    ESP_ERROR_CHECK(ret);

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
                        alloc_offline_buffer(idx, OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED);
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
            redirect_handler(req);
            break;

        case WEBFILESERVER_CMD_DL:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_DL");
            if (!is_available){
                snprintf(resp_str, 128, "Download requested for device %s, not available",  device_name_set?device_name:"n/a");
                httpd_resp_send(req, resp_str, strlen(resp_str));
            } else {
                http_resp_csv_download(req, idx);
            }
            break;

        case WEBFILESERVER_CMD_RESET:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_RESET");
            status = ble_beacons[idx].offline_buffer_status;
            switch(status){
                case OFFLINE_BUFFER_STATUS_NONE:
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED:
                    ESP_LOGD(TAG, "set to status none");
                    free_offline_buffer(idx, OFFLINE_BUFFER_STATUS_NONE);
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS:
                    ESP_LOGD(TAG, "already in progress, wait for now, give up next time possible...");
                    gattc_give_up_now = true;
                    break;
                case OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE:
                    ESP_LOGD(TAG, "already available for download");
                    free_offline_buffer(idx, OFFLINE_BUFFER_STATUS_NONE);
                    break;
                default:
                    ESP_LOGD(TAG, "unhandled switch case");
                    break;
            }
            redirect_handler(req);
            break;

        case WEBFILESERVER_CMD_LIST:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_LIST");
            http_resp_list_devices(req);
            break;

        case WEBFILESERVER_CMD_OTA:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_OTA");
            redirect_handler(req);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            xEventGroupSetBits(ota_evg, OTA_START_UPDATE);
            break;

        case WEBFILESERVER_CMD_REBOOT:
            ESP_LOGD(TAG, "csv_get_handler WEBFILESERVER_CMD_REBOOT");
            redirect_handler(req);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            ESP_LOGE(TAG, "reboot flag is set -> esp_restart() -> REBOOT");
            fflush(stdout);
            esp_restart();

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
