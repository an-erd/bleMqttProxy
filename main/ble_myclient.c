#include "blemqttproxy.h"

#include "helperfunctions.h"
#include "esp_log.h"
#include "ble_myclient.h"
#include "beacon.h"

static const char* TAG = "BLE_MYCLIENT";

// We're connecting only to one device type, the ble_beacon
#define PROFILE_CLIENT_NUM                      1
#define GATTC_NOTIFY_CHAR_UUID_BEACON           0x1401
#define GATTC_INDICATION_CHAR_UUID_BEACON       0x2A52
#define GATTC_NOTIFY_HANDLE_BEACON              0x26
#define GATTC_INDICATE_HANDLE_BEACON            0x2B

struct gattc_profile_inst {
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t char_handle_1401;
    uint16_t char_handle_2A52;
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_client_tab[PROFILE_CLIENT_NUM] = {
    [PROFILE_ID_BEACON] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE,       /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};


const uint8_t REMOTE_SERVICE_UUID[] = {0x3C, 0xB7, 0xA2, 0x4B, 0x0C, 0x32, 0xF2, 0x9F, 0x4F, 0x4C, 0xF5, 0x37, 0x00, 0x14, 0x2F, 0x61};

bool gattc_connect = false;
bool gattc_is_connected = false;
bool gattc_scanning = false;
bool gattc_give_up_now = false;
uint8_t gattc_connect_beacon_idx = UNKNOWN_BEACON;
bool gattc_offline_buffer_downloading = false;

bool get_server = false;
bool device_notify_1401 = false;
bool device_indicate_2A52 = false;


esp_bt_uuid_t remote_filter_service_uuid;

esp_bt_uuid_t remote_filter_char_uuid_1401 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = GATTC_NOTIFY_CHAR_UUID_BEACON,},
};

esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

esp_bt_uuid_t remote_filter_char_uuid_2A52 = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = GATTC_INDICATION_CHAR_UUID_BEACON,},
};

esp_bt_uuid_t indication_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,},
};

void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event) {
    case ESP_GATTC_REG_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_REG_EVT");
        esp_ble_gap_config_local_privacy(true);
        break;
    case ESP_GATTC_CONNECT_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_CONNECT_EVT conn_id %d, if %d", p_data->connect.conn_id, gattc_if);
        gl_profile_client_tab[PROFILE_ID_BEACON].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_client_tab[PROFILE_ID_BEACON].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGD(TAG, "REMOTE BDA:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, gl_profile_client_tab[PROFILE_ID_BEACON].remote_bda, sizeof(esp_bd_addr_t), ESP_LOG_DEBUG);
        gattc_is_connected = true;
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req (gattc_if, p_data->connect.conn_id);
        if (mtu_ret){
            ESP_LOGE(TAG, "config MTU error, error code = %x", mtu_ret);
        }
        break;
    }
    case ESP_GATTC_OPEN_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_OPEN_EVT");
        if (param->open.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGD(TAG, "open success");
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_CFG_MTU_EVT");
        if (param->cfg_mtu.status != ESP_GATT_OK){
            ESP_LOGE(TAG,"config mtu failed, error status = %x", param->cfg_mtu.status);
        }
        ESP_LOGD(TAG, "ESP_GATTC_CFG_MTU_EVT, Status %d, MTU %d, conn_id %d", param->cfg_mtu.status, param->cfg_mtu.mtu, param->cfg_mtu.conn_id);
        ESP_LOGD(TAG, "wait now until ESP_GATTS_RESPONSE_EVT");
        break;
    case ESP_GATTC_SEARCH_RES_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_RES_EVT");
        ESP_LOGD(TAG, "SEARCH RES: conn_id = %x is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGD(TAG, "start handle %d end handle %d current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16){
            ESP_LOGD(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_32){
            ESP_LOGD(TAG, "UUID32: %x", p_data->search_res.srvc_id.uuid.uuid.uuid32);
        } else if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_128){
            esp_gatt_id_t *srvc_id = &p_data->search_res.srvc_id;
            ESP_LOGD(TAG, "UUID128: %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x",       srvc_id->uuid.uuid.uuid128[0],
                     srvc_id->uuid.uuid.uuid128[1], srvc_id->uuid.uuid.uuid128[2], srvc_id->uuid.uuid.uuid128[3],
                     srvc_id->uuid.uuid.uuid128[4], srvc_id->uuid.uuid.uuid128[5], srvc_id->uuid.uuid.uuid128[6],
                     srvc_id->uuid.uuid.uuid128[7], srvc_id->uuid.uuid.uuid128[8], srvc_id->uuid.uuid.uuid128[9],
                     srvc_id->uuid.uuid.uuid128[10], srvc_id->uuid.uuid.uuid128[11], srvc_id->uuid.uuid.uuid128[12],
                     srvc_id->uuid.uuid.uuid128[13], srvc_id->uuid.uuid.uuid128[14], srvc_id->uuid.uuid.uuid128[15]);
            ESP_LOGD(TAG, "service found, start_handle 0x%X end_handle 0x%X", p_data->search_res.start_handle, p_data->search_res.end_handle);
            get_server = true;
            gl_profile_client_tab[PROFILE_ID_BEACON].service_start_handle = p_data->search_res.start_handle;
            gl_profile_client_tab[PROFILE_ID_BEACON].service_end_handle = p_data->search_res.end_handle;
        }
        break;
    }
    case ESP_GATTC_SEARCH_CMPL_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        if (p_data->search_cmpl.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "search service failed, error status = %x", p_data->search_cmpl.status);
            break;
        }
        if(p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE) {
            ESP_LOGD(TAG, "Get service information from remote device");
        } else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH) {
            ESP_LOGD(TAG, "Get service information from flash");
        } else {
            ESP_LOGW(TAG, "unknown service source");
        }
        ESP_LOGD(TAG, "ESP_GATTC_SEARCH_CMPL_EVT");
        ESP_LOGD(TAG, "get_server %d", get_server);
        if (get_server){
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                     p_data->search_cmpl.conn_id,
                                                                     ESP_GATT_DB_CHARACTERISTIC,
                                                                     gl_profile_client_tab[PROFILE_ID_BEACON].service_start_handle,
                                                                     gl_profile_client_tab[PROFILE_ID_BEACON].service_end_handle,
                                                                     INVALID_HANDLE,
                                                                     &count);
            if (status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            ESP_LOGD(TAG, "count %d", count);
            if (count > 0){
                esp_gattc_char_elem_t *char_elem_result = NULL;
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result){
                    ESP_LOGE(TAG, "gattc no mem");
                }else{
                    // 1401
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_client_tab[PROFILE_ID_BEACON].service_start_handle,
                                                             gl_profile_client_tab[PROFILE_ID_BEACON].service_end_handle,
                                                             remote_filter_char_uuid_1401,
                                                             char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY)){
                        gl_profile_client_tab[PROFILE_ID_BEACON].char_handle_1401 = char_elem_result[0].char_handle;
                        ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with notify", char_elem_result[0].char_handle);
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_client_tab[PROFILE_ID_BEACON].remote_bda, char_elem_result[0].char_handle);
                    }

                    // 2A52
                    status = esp_ble_gattc_get_char_by_uuid( gattc_if,
                                                             p_data->search_cmpl.conn_id,
                                                             gl_profile_client_tab[PROFILE_ID_BEACON].service_start_handle,
                                                             gl_profile_client_tab[PROFILE_ID_BEACON].service_end_handle,
                                                             remote_filter_char_uuid_2A52,
                                                             char_elem_result,
                                                             &count);
                    if (status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                    }

                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_INDICATE)){
                        gl_profile_client_tab[PROFILE_ID_BEACON].char_handle_2A52 = char_elem_result[0].char_handle;
                        ESP_LOGD(TAG, "esp_ble_gattc_register_for_notify 0x%X, found with indicate", char_elem_result[0].char_handle);
                        esp_ble_gattc_register_for_notify (gattc_if, gl_profile_client_tab[PROFILE_ID_BEACON].remote_bda, char_elem_result[0].char_handle);
                    }
                }
                /* free char_elem_result */
                free(char_elem_result);
            }else{
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT");
        if (p_data->reg_for_notify.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "REG FOR NOTIFY failed: error status = %d", p_data->reg_for_notify.status);
        }else{
            uint16_t count          = 0;
            uint16_t notify_en      = 1;
            uint16_t indicate_en    = 2;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count( gattc_if,
                                                                         gl_profile_client_tab[PROFILE_ID_BEACON].conn_id,
                                                                         ESP_GATT_DB_DESCRIPTOR,
                                                                         gl_profile_client_tab[PROFILE_ID_BEACON].service_start_handle,
                                                                         gl_profile_client_tab[PROFILE_ID_BEACON].service_end_handle,
                                                                         gl_profile_client_tab[PROFILE_ID_BEACON].char_handle_1401,
                                                                         &count);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
            }
            ESP_LOGD(TAG, "ESP_GATTC_REG_FOR_NOTIFY_EVT count = %d", count);
            if (count > 0){
                esp_gattc_descr_elem_t *descr_elem_result = NULL;
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result){
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                }else{
                    ret_status = esp_ble_gattc_get_descr_by_char_handle( gattc_if,
                                                                         gl_profile_client_tab[PROFILE_ID_BEACON].conn_id,
                                                                         p_data->reg_for_notify.handle,
                                                                         notify_descr_uuid,
                                                                         descr_elem_result,
                                                                         &count);
                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                    }

                    ESP_LOGD(TAG, "esp_ble_gattc_write_char_descr (notify) 0x%X", descr_elem_result[0].handle);

                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG){
                        if (descr_elem_result[0].handle == 0x26){
                            ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                        gl_profile_client_tab[PROFILE_ID_BEACON].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(notify_en),
                                                                        (uint8_t *)&notify_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                        } else if (descr_elem_result[0].handle == 0x2B){
                            ret_status = esp_ble_gattc_write_char_descr( gattc_if,
                                                                        gl_profile_client_tab[PROFILE_ID_BEACON].conn_id,
                                                                        descr_elem_result[0].handle,
                                                                        sizeof(indicate_en),
                                                                        (uint8_t *)&indicate_en,
                                                                        ESP_GATT_WRITE_TYPE_RSP,
                                                                        ESP_GATT_AUTH_REQ_NONE);
                        }
                    }

                    if (ret_status != ESP_GATT_OK){
                        ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else{
                ESP_LOGE(TAG, "decsr not found");
            }

        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT: {
        // ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT");

        static int64_t time_measure = 0;
        uint8_t len = 0;
        uint16_t count = 0;
        uint8_t idx = gattc_connect_beacon_idx;

        if (p_data->notify.is_notify){
            count = ble_beacons[idx].offline_buffer_count;
            if(!count){
                ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT, first, idx %d, ble_beacons[idx] %d, ble_beacons[idx].p_buffer_download[count] %d",
                    idx, (&ble_beacons[idx] != NULL),  (ble_beacons[idx].p_buffer_download != NULL) );
                time_measure = esp_timer_get_time();
            } else {
                ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, subsequent, count %d", count);
            }
            ble_beacons[idx].p_buffer_download[count].sequence_number  = uint16_decode(&p_data->notify.value[len]); len += 2; // uint16_t
            ble_beacons[idx].p_buffer_download[count].time_stamp       = uint32_decode(&p_data->notify.value[len]); len += 4; // time_t
            ble_beacons[idx].p_buffer_download[count].temperature_f
                = SHT3_GET_TEMPERATURE_VALUE(p_data->notify.value[len], p_data->notify.value[len+1]);
            len += 2;
            ble_beacons[idx].p_buffer_download[count].humidity_f
                = SHT3_GET_HUMIDITY_VALUE(p_data->notify.value[len], p_data->notify.value[len+1]);
            len += 2;
            // ESP_LOG_BUFFER_HEX_LEVEL(TAG, &ble_beacons[idx].p_buffer_download[count], p_data->notify.value_len, ESP_LOG_DEBUG);
            ble_beacons[idx].offline_buffer_count++;
        } else {
            ESP_LOGD(TAG, "ESP_GATTC_NOTIFY_EVT, receive indicate value:");
            ESP_LOGI(TAG, "ESP_GATTC_NOTIFY_EVT done, received %d, time difference %d ms",
                ble_beacons[idx].offline_buffer_count, (uint16_t) ((esp_timer_get_time() - time_measure)/1000.));
        }

        if (!p_data->notify.is_notify){
            ESP_LOGD(TAG, "now disconnecting");
            esp_gatt_status_t ret_status = esp_ble_gattc_close(gl_profile_client_tab[PROFILE_ID_BEACON].gattc_if, 0);
            if (ret_status != ESP_GATT_OK){
                ESP_LOGE(TAG, "esp_ble_gattc_close, error status %d", ret_status);
            }
            ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_DOWNLOAD_AVAILABLE;
            gattc_offline_buffer_downloading = false;
        }
        break;
    }
    case ESP_GATTC_WRITE_DESCR_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_WRITE_DESCR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write descr failed, error status = %x", p_data->write.status);
            break;
        }
        ESP_LOGD(TAG, "write descr success handle 0x%X", p_data->write.handle);
        if(p_data->write.handle == GATTC_NOTIFY_HANDLE_BEACON){
            device_notify_1401 = true;
        } else if(p_data->write.handle == GATTC_INDICATE_HANDLE_BEACON){
            device_indicate_2A52 = true;
        }

        if (! (device_notify_1401 && device_indicate_2A52) )
            break;

        uint8_t write_char_data[2];
        write_char_data[0] = 0x01;
        write_char_data[1] = 0x01;      // TODO send last, send all, ...

        ESP_LOGD(TAG, "esp_ble_gattc_write_char 0x%X", gl_profile_client_tab[PROFILE_ID_BEACON].char_handle_2A52);
        esp_ble_gattc_write_char( gattc_if,
                                  gl_profile_client_tab[PROFILE_ID_BEACON].conn_id,
                                  gl_profile_client_tab[PROFILE_ID_BEACON].char_handle_2A52,
                                  sizeof(write_char_data),
                                  write_char_data,
                                  ESP_GATT_WRITE_TYPE_RSP,
                                  ESP_GATT_AUTH_REQ_NO_MITM);       // ESP_GATT_AUTH_REQ_NONE);
        break;
    case ESP_GATTC_SRVC_CHG_EVT: {
        ESP_LOGD(TAG, "ESP_GATTC_SRVC_CHG_EVT");
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGD(TAG, "ESP_GATTC_SRVC_CHG_EVT, bd_addr:");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, bda, sizeof(esp_bd_addr_t), ESP_LOG_DEBUG);
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_WRITE_CHAR_EVT");
        if (p_data->write.status != ESP_GATT_OK){
            ESP_LOGE(TAG, "write char failed, error status = %x, error = %s", p_data->write.status, esp_err_to_name(p_data->write.status) );
            break;
        }
        ESP_LOGD(TAG, "write char success ");
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        ESP_LOGD(TAG, "ESP_GATTC_DISCONNECT_EVT");
        if(gattc_offline_buffer_downloading) {
            if(gattc_give_up_now == true){
                free_offline_buffer(gattc_connect_beacon_idx, OFFLINE_BUFFER_STATUS_NONE);
                gattc_give_up_now = false;
                gattc_offline_buffer_downloading = false;
            } else {
                // disconnect occured but download not completed, maybe next time
                reset_offline_buffer(gattc_connect_beacon_idx, OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED);
                gattc_offline_buffer_downloading = false;
            }
        }

        gattc_connect = false;
        gattc_is_connected = false;
        gattc_connect_beacon_idx = UNKNOWN_BEACON;
        gattc_give_up_now = false;
        get_server = false;
        device_notify_1401 = false;
        device_indicate_2A52 = false;

        ESP_LOGI(TAG, "ESP_GATTC_DISCONNECT_EVT, reason = %d", p_data->disconnect.reason);

        uint32_t duration = 0;  // scan permanently
        esp_ble_gap_start_scanning(duration);
        break;
    default:
        break;
    }
}

void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT) {
        ESP_LOGD(TAG, "esp_gattc_cb ESP_GATTC_REG_EVT");
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_client_tab[param->reg.app_id].gattc_if = gattc_if;
        } else {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                   param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_CLIENT_NUM; idx++) {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                    gattc_if == gl_profile_client_tab[idx].gattc_if) {
                if (gl_profile_client_tab[idx].gattc_cb) {
                    gl_profile_client_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void gattc_search_service_beacon()
{
    esp_ble_gattc_search_service(gl_profile_client_tab[PROFILE_ID_BEACON].gattc_if, gl_profile_client_tab[PROFILE_ID_BEACON].conn_id, &remote_filter_service_uuid);
}

void gattc_open_beacon(esp_ble_gap_cb_param_t *rst)
{
    esp_ble_gattc_open(gl_profile_client_tab[PROFILE_ID_BEACON].gattc_if, rst->scan_rst.bda, rst->scan_rst.ble_addr_type, true);
}
