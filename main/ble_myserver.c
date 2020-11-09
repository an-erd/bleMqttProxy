#include "blemqttproxy.h"

#include "helperfunctions.h"
#include "ble_myserver.h"
#include "ble_myclient.h"
#include "ble_sntp.h"

static const char* TAG = "BLE_MYSERVER";

/*  We're offering only the CTS (current time service) service to the peripheral. */
#define PROFILE_SERVER_NUM              1
#define GATTS_SERVICE_UUID_CTS          0x1805
#define GATTS_CHAR_UUID_CTS             0x2A2B
#define GATTS_NUM_HANDLE_CTS            4
#define GATTS_CTS_CHAR_VAL_LEN_MAX      0x10

struct gatts_profile_inst {
    esp_gatts_cb_t gatts_cb;
    uint16_t gatts_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_handle;
    esp_gatt_srvc_id_t service_id;
    uint16_t char_handle;
    esp_bt_uuid_t char_uuid;
    esp_gatt_perm_t perm;
    esp_gatt_char_prop_t property;
    uint16_t descr_handle;
    esp_bt_uuid_t descr_uuid;
};

/* Char return value, see ESP_GATTS_READ_EVT on how to build it. */
static uint8_t char1_str[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, };

esp_attr_value_t gatts_cts_char1_val =
{
    .attr_max_len = GATTS_CTS_CHAR_VAL_LEN_MAX,
    .attr_len     = sizeof(char1_str),
    .attr_value   = char1_str,
};

/* One gatt-based profile one app_id and one gatts_if, this array will store the gatts_if returned by ESP_GATTS_REG_EVT */
static struct gatts_profile_inst gl_profile_server_tab[PROFILE_SERVER_NUM] = {
    [PROFILE_ID_CTS] = {
        .gatts_cb = gatts_profile_event_handler,
        .gatts_if = ESP_GATT_IF_NONE,
    },
};

void gatts_profile_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
    case ESP_GATTS_REG_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_REG_EVT, status %d, app_id %d", param->reg.status, param->reg.app_id);
        gl_profile_server_tab[PROFILE_ID_CTS].service_id.is_primary = true;
        gl_profile_server_tab[PROFILE_ID_CTS].service_id.id.inst_id = 0x00;
        gl_profile_server_tab[PROFILE_ID_CTS].service_id.id.uuid.len = ESP_UUID_LEN_16;
        gl_profile_server_tab[PROFILE_ID_CTS].service_id.id.uuid.uuid.uuid16 = GATTS_SERVICE_UUID_CTS;
        esp_ble_gatts_create_service(gatts_if, &gl_profile_server_tab[PROFILE_ID_CTS].service_id, GATTS_NUM_HANDLE_CTS);
        break;
    case ESP_GATTS_READ_EVT: {
        ESP_LOGD(TAG, "ESP_GATTS_READ_EVT, conn_id %d, trans_id %d, handle %d", param->read.conn_id, param->read.trans_id, param->read.handle);

        struct tm timeinfo;
        struct timeval tv_now;

        gettimeofday(&tv_now, NULL);
        localtime_r(&tv_now.tv_sec, &timeinfo);

        esp_gatt_rsp_t rsp;
        memset(&rsp, 0, sizeof(esp_gatt_rsp_t));
        rsp.attr_value.handle = param->read.handle;
        rsp.attr_value.len = 10;

        /**< |     Year        |Month   |Day     |Hours   |Minutes |Seconds |Weekday |Fraction|Reason  |
             |     2 bytes     |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  |1 byte  | = 10 bytes.

          Example: C2 07 0B 0F 0D 25 2A 06 FE 08
                0x07C2  1986
                0x0B    11
                0x0F    15
                0x0D    13
                0x25    37
                0x2A    42
                0x06    Saturday
                0xFE    254/256
                0x08    Daylight savings 1, Time zone 0, External update 0, Manual update 0
        **/
       if(sntp_time_available){
            uint8_t len = uint16_encode(timeinfo.tm_year + 1900, rsp.attr_value.value);
            rsp.attr_value.value[len++] =  timeinfo.tm_mon + 1;
            rsp.attr_value.value[len++] =  timeinfo.tm_mday;
            rsp.attr_value.value[len++] =  timeinfo.tm_hour;
            rsp.attr_value.value[len++] =  timeinfo.tm_min;
            rsp.attr_value.value[len++] =  timeinfo.tm_sec;
            rsp.attr_value.value[len++] =  timeinfo.tm_wday;

            int16_t time_ms = (int16_t) (tv_now.tv_usec / 1000);
            rsp.attr_value.value[len++] =  (time_ms * 256 / 999)%256;     // Fractions of a second.

            // // Reason for updating the time.
            // p_time->adjust_reason.manual_time_update              = (p_data[index] >> 0) & 0x01;
            // p_time->adjust_reason.external_reference_time_update  = (p_data[index] >> 1) & 0x01;
            // p_time->adjust_reason.change_of_time_zone             = (p_data[index] >> 2) & 0x01;
            // p_time->adjust_reason.change_of_daylight_savings_time = (p_data[index] >> 3) & 0x01;
       } else {
           // send only "0" which will give an error on the peripheral side
       }
        esp_ble_gatts_send_response(gatts_if, param->read.conn_id, param->read.trans_id,
                                    ESP_GATT_OK, &rsp);
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, (void *)rsp.attr_value.value, 10* sizeof(uint8_t), ESP_LOG_DEBUG);
        break;
    }
    case ESP_GATTS_RESPONSE_EVT: {
        ESP_LOGD(TAG, "ESP_GATTS_RESPONSE_EVT, status %d, handle %d", param->rsp.status, param->rsp.handle);

        ESP_LOGD(TAG, "calling esp_ble_gattc_search_service to continue bulk download");
        gattc_search_service_beacon();

        break;
    }
    case ESP_GATTS_WRITE_EVT: {
        ESP_LOGD(TAG, "ESP_GATTS_WRITE_EVT, conn_id %d, trans_id %d, handle %d", param->write.conn_id, param->write.trans_id, param->write.handle);
        break;
    }
    case ESP_GATTS_EXEC_WRITE_EVT:
        ESP_LOGD(TAG,"ESP_GATTS_EXEC_WRITE_EVT");
        break;
    case ESP_GATTS_MTU_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_MTU_EVT, MTU %d", param->mtu.mtu);
        break;
    case ESP_GATTS_UNREG_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_UNREG_EVT");
        break;
    case ESP_GATTS_CREATE_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CREATE_EVT, status %d,  service_handle %d", param->create.status, param->create.service_handle);
        gl_profile_server_tab[PROFILE_ID_CTS].service_handle = param->create.service_handle;
        gl_profile_server_tab[PROFILE_ID_CTS].char_uuid.len = ESP_UUID_LEN_16;
        gl_profile_server_tab[PROFILE_ID_CTS].char_uuid.uuid.uuid16 = GATTS_CHAR_UUID_CTS;

        esp_ble_gatts_start_service(gl_profile_server_tab[PROFILE_ID_CTS].service_handle);
        esp_err_t add_char_ret = esp_ble_gatts_add_char(gl_profile_server_tab[PROFILE_ID_CTS].service_handle, &gl_profile_server_tab[PROFILE_ID_CTS].char_uuid,
                                                        ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
                                                        ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY,
                                                        &gatts_cts_char1_val, NULL);
        if (add_char_ret){
            ESP_LOGE(TAG, "add char failed, error code =%x",add_char_ret);
        }
        break;
    case ESP_GATTS_ADD_INCL_SRVC_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_ADD_INCL_SRVC_EVT");
        break;
    case ESP_GATTS_ADD_CHAR_EVT: {
        uint16_t length = 0;
        const uint8_t *prf_char;

        ESP_LOGD(TAG, "ESP_GATTS_ADD_CHAR_EVT, status %d, attr_handle %d, service_handle %d",
                param->add_char.status, param->add_char.attr_handle, param->add_char.service_handle);
        gl_profile_server_tab[PROFILE_ID_CTS].char_handle = param->add_char.attr_handle;
        gl_profile_server_tab[PROFILE_ID_CTS].descr_uuid.len = ESP_UUID_LEN_16;
        gl_profile_server_tab[PROFILE_ID_CTS].descr_uuid.uuid.uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG;
        esp_err_t get_attr_ret = esp_ble_gatts_get_attr_value(param->add_char.attr_handle,  &length, &prf_char);
        if (get_attr_ret == ESP_FAIL){
            ESP_LOGE(TAG, "ILLEGAL HANDLE");
        }

        ESP_LOGD(TAG, "ESP_GATTS_ADD_CHAR_EVT char length = %x", length);
        for(int i = 0; i < length; i++){
            ESP_LOGD(TAG, "prf_char[%x] =%x", i, prf_char[i]);
        }
        esp_err_t add_descr_ret = esp_ble_gatts_add_char_descr(gl_profile_server_tab[PROFILE_ID_CTS].service_handle, &gl_profile_server_tab[PROFILE_ID_CTS].descr_uuid,
                                                                ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE, NULL, NULL);
        if (add_descr_ret){
            ESP_LOGE(TAG, "add char descr failed, error code =%x", add_descr_ret);
        }
        break;
    }
    case ESP_GATTS_ADD_CHAR_DESCR_EVT:
        gl_profile_server_tab[PROFILE_ID_CTS].descr_handle = param->add_char_descr.attr_handle;
        ESP_LOGD(TAG, "ESP_GATTS_ADD_CHAR_DESCR_EVT, status %d, attr_handle %d, service_handle %d",
                 param->add_char_descr.status, param->add_char_descr.attr_handle, param->add_char_descr.service_handle);
        break;
    case ESP_GATTS_DELETE_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_DELETE_EVT");
        break;
    case ESP_GATTS_START_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_START_EVT, status %d, service_handle %d",
                 param->start.status, param->start.service_handle);
        break;
    case ESP_GATTS_STOP_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_STOP_EVT");
        break;
    case ESP_GATTS_CONNECT_EVT: {
        esp_ble_conn_update_params_t conn_params = {0};
        memcpy(conn_params.bda, param->connect.remote_bda, sizeof(esp_bd_addr_t));
        /* For the IOS system, please reference the apple official documents about the ble connection parameters restrictions. */
        conn_params.latency = 0;
        conn_params.max_int = 0x20;     // max_int = 0x20*1.25ms = 40ms
        conn_params.min_int = 0x10;     // min_int = 0x10*1.25ms = 20ms
        conn_params.timeout = 400;      // timeout = 400*10ms = 4000ms
        ESP_LOGD(TAG, "ESP_GATTS_CONNECT_EVT, conn_id %d, remote %02x:%02x:%02x:%02x:%02x:%02x:",
                 param->connect.conn_id,
                 param->connect.remote_bda[0], param->connect.remote_bda[1], param->connect.remote_bda[2],
                 param->connect.remote_bda[3], param->connect.remote_bda[4], param->connect.remote_bda[5]);
        gl_profile_server_tab[PROFILE_ID_CTS].conn_id = param->connect.conn_id;
        //start sent the update connection parameters to the peer device.
        esp_ble_gap_update_conn_params(&conn_params);
        break;
    }
    case ESP_GATTS_DISCONNECT_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_DISCONNECT_EVT, disconnect reason 0x%x", param->disconnect.reason);
        break;
    case ESP_GATTS_CONF_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CONF_EVT, status %d attr_handle %d", param->conf.status, param->conf.handle);
        if (param->conf.status != ESP_GATT_OK){
            esp_log_buffer_hex(TAG, param->conf.value, param->conf.len);
        }
        break;
    case ESP_GATTS_OPEN_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_OPEN_EVT");
        break;
    case ESP_GATTS_CANCEL_OPEN_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CANCEL_OPEN_EVT");
        break;
    case ESP_GATTS_CLOSE_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CLOSE_EVT");
        break;
    case ESP_GATTS_LISTEN_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_LISTEN_EVT");
        break;
    case ESP_GATTS_CONGEST_EVT:
        ESP_LOGD(TAG, "ESP_GATTS_CONGEST_EVT");
        break;
    default:
        break;
    }
}

void esp_gatts_cb(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param)
{
    ESP_LOGD(TAG, "esp_gatts_cb: evt %d, gatt_if %d", event, gatts_if);

    /* If event is register event, store the gatts_if for each profile */
    if (event == ESP_GATTS_REG_EVT) {
        ESP_LOGD(TAG, "ESP_GATTS_REG_EVT");
        if (param->reg.status == ESP_GATT_OK) {
            gl_profile_server_tab[param->reg.app_id].gatts_if = gatts_if;
        } else {
            ESP_LOGE(TAG, "Reg app failed, app_id %04x, status %d",
                    param->reg.app_id,
                    param->reg.status);
            return;
        }
    }

    /* If the gatts_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do {
        int idx;
        for (idx = 0; idx < PROFILE_SERVER_NUM; idx++) {
            if (gatts_if == ESP_GATT_IF_NONE||gatts_if == gl_profile_server_tab[idx].gatts_if) 			{
                if (gl_profile_server_tab[idx].gatts_cb) {
                    ESP_LOGD(TAG, "esp_gatts_cb: forward evt %d", event);
                    gl_profile_server_tab[idx].gatts_cb(event, gatts_if, param);
                }
            }
        }
    } while (0);
}
