#include "esp_gatt_defs.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"

#include "ble.h"
#include "ble_myclient.h"
#include "ble_myserver.h"
#include "beacon.h"
#include "display.h"
#include "ble_mqtt.h"

static const char *TAG = "BLE";

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static const char *esp_key_type_to_str(esp_ble_key_type_t key_type)
{
   const char *key_str = NULL;
   switch(key_type) {
    case ESP_LE_KEY_NONE:
        key_str = "ESP_LE_KEY_NONE";
        break;
    case ESP_LE_KEY_PENC:
        key_str = "ESP_LE_KEY_PENC";
        break;
    case ESP_LE_KEY_PID:
        key_str = "ESP_LE_KEY_PID";
        break;
    case ESP_LE_KEY_PCSRK:
        key_str = "ESP_LE_KEY_PCSRK";
        break;
    case ESP_LE_KEY_PLK:
        key_str = "ESP_LE_KEY_PLK";
        break;
    case ESP_LE_KEY_LLK:
        key_str = "ESP_LE_KEY_LLK";
        break;
    case ESP_LE_KEY_LENC:
        key_str = "ESP_LE_KEY_LENC";
        break;
    case ESP_LE_KEY_LID:
        key_str = "ESP_LE_KEY_LID";
        break;
    case ESP_LE_KEY_LCSRK:
        key_str = "ESP_LE_KEY_LCSRK";
        break;
    default:
        key_str = "INVALID BLE KEY TYPE";
        break;

    }
     return key_str;
}

static char *esp_auth_req_to_str(esp_ble_auth_req_t auth_req)
{
   char *auth_str = NULL;
   switch(auth_req) {
    case ESP_LE_AUTH_NO_BOND:
        auth_str = "ESP_LE_AUTH_NO_BOND";
        break;
    case ESP_LE_AUTH_BOND:
        auth_str = "ESP_LE_AUTH_BOND";
        break;
    case ESP_LE_AUTH_REQ_MITM:
        auth_str = "ESP_LE_AUTH_REQ_MITM";
        break;
    case ESP_LE_AUTH_REQ_BOND_MITM:
        auth_str = "ESP_LE_AUTH_REQ_BOND_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_ONLY:
        auth_str = "ESP_LE_AUTH_REQ_SC_ONLY";
        break;
    case ESP_LE_AUTH_REQ_SC_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_BOND";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM";
        break;
    case ESP_LE_AUTH_REQ_SC_MITM_BOND:
        auth_str = "ESP_LE_AUTH_REQ_SC_MITM_BOND";
        break;
    default:
        auth_str = "INVALID BLE AUTH REQ";
        break;
   }

   return auth_str;
}

void __attribute__((unused)) show_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    ESP_LOGI(TAG, "Bonded devices number : %d", dev_num);
    for (int i = 0; i < dev_num; i++) {
        ESP_LOGI(TAG, "Bond device num %d,", i);
        ESP_LOGI(TAG, "bd_addr");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, (void *)dev_list[i].bd_addr, sizeof(esp_bd_addr_t), ESP_LOG_INFO);
        ESP_LOGI(TAG, "bond_key.pid_key.irk");
        ESP_LOG_BUFFER_HEX_LEVEL(TAG, (void *)dev_list[i].bond_key.pid_key.irk, sizeof(esp_bt_octet16_t), ESP_LOG_INFO);
    }

    free(dev_list);
}

void __attribute__((unused)) remove_all_bonded_devices(void)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
    for (int i = 0; i < dev_num; i++) {
        esp_ble_remove_bond_device(dev_list[i].bd_addr);
    }

    free(dev_list);
}

void remove_bonded_devices_num(uint8_t num_bond_device)
{
    int dev_num = esp_ble_get_bond_device_num();

    esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *)malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
    esp_ble_get_bond_device_list(&dev_num, dev_list);
        esp_ble_remove_bond_device(dev_list[num_bond_device].bd_addr);

    free(dev_list);
}

void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    esp_err_t err;

    switch (event) {
    case ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SET_LOCAL_PRIVACY_COMPLETE_EVT");
        if (param->local_privacy_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "config local privacy failed, error code =%x", param->local_privacy_cmpl.status);
            break;
        }
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret){
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT");
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        uint32_t duration = 0;  // scan permanently
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        //scan start complete event to indicate scan start successfully or failed
        if ((err = param->scan_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Scan start failed: %s", esp_err_to_name(err));
        }
        ESP_LOGD(TAG, "scan start success");
        gattc_scanning = true;

        // if(web_file_server_temporarily_stopped){
        //     ESP_LOGD(TAG, "restart temporarily stopped web server");
        //     web_server = start_webserver();
        // }

        break;
    case ESP_GAP_BLE_PASSKEY_REQ_EVT:                           /* passkey request event */
        /* Call the following function to input the passkey which is displayed on the remote device */
        ESP_LOGD(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT");
        break;
    case ESP_GAP_BLE_OOB_REQ_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_OOB_REQ_EVT");
        uint8_t tk[16] = {1}; //If you paired with OOB, both devices need to use the same tk
        esp_ble_oob_req_reply(param->ble_security.ble_req.bd_addr, tk, sizeof(tk));
        break;
    }
    case ESP_GAP_BLE_LOCAL_IR_EVT:                               /* BLE local IR event */
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_IR_EVT");
        break;
    case ESP_GAP_BLE_LOCAL_ER_EVT:                               /* BLE local ER event */
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        break;
    case ESP_GAP_BLE_SEC_REQ_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_LOCAL_ER_EVT");
        /* send the positive(true) security response to the peer device to accept the security request.
        If not accept the security request, should send the security response with negative(false) accept value*/
        esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
        break;
    case ESP_GAP_BLE_NC_REQ_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_NC_REQ_EVT");
        /* The app will receive this evt when the IO has DisplayYesNO capability and the peer device IO also has DisplayYesNo capability.
        show the passkey number to the user to confirm it with the number displayed by peer device. */
        esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
        ESP_LOGD(TAG, "ESP_GAP_BLE_NC_REQ_EVT, the passkey Notify number:%d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT:  ///the app will receive this evt when the IO  has Output capability and the peer device IO has Input capability.
        ESP_LOGD(TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT");
        ///show the passkey number to the user to input it in the peer device.
        ESP_LOGI(TAG, "The passkey Notify number:%06d", param->ble_security.key_notif.passkey);
        break;
    case ESP_GAP_BLE_KEY_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_KEY_EVT");
        //shows the ble key info share with peer device to the user.
        ESP_LOGD(TAG, "key type = %s", esp_key_type_to_str(param->ble_security.ble_key.key_type));
        break;
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
        ESP_LOGD(TAG, "ESP_GAP_BLE_AUTH_CMPL_EVT");
        esp_bd_addr_t bd_addr;
        memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "remote BD_ADDR: %08x%04x",\
                (bd_addr[0] << 24) + (bd_addr[1] << 16) + (bd_addr[2] << 8) + bd_addr[3],
                (bd_addr[4] << 8) + bd_addr[5]);
        ESP_LOGI(TAG, "address type = %d", param->ble_security.auth_cmpl.addr_type);
        ESP_LOGI(TAG, "pair status = %s",param->ble_security.auth_cmpl.success ? "success" : "fail");
        if (!param->ble_security.auth_cmpl.success) {
            ESP_LOGI(TAG, "fail reason = 0x%x",param->ble_security.auth_cmpl.fail_reason);
        } else {
            ESP_LOGI(TAG, "auth mode = %s",esp_auth_req_to_str(param->ble_security.auth_cmpl.auth_mode));
        }

        break;
    }
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_START_COMPLETE_EVT");
        //adv start complete event to indicate adv start successfully or failed
        if ((err = param->adv_start_cmpl.status) != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(TAG, "Adv start failed: %s", esp_err_to_name(err));
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        // ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT");
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;

        beacon_type_t beacon_type = UNKNOWN_BEACON;
        uint16_t maj = 0, min = 0, battery = 0;
        int16_t x = 0, y = 0, z = 0;
        uint8_t idx = 0;
        float temp = 0, humidity = 0;

        bool is_beacon_active = true;
        bool is_beacon_close = false;
        bool mqtt_send_adv = false;

        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            // ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT");

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0) {
                ESP_LOGD(TAG, "adv data:");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len, ESP_LOG_DEBUG);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0) {
                ESP_LOGD(TAG, "scan resp:");
                ESP_LOG_BUFFER_HEX_LEVEL(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len, ESP_LOG_DEBUG);
            }
#endif

            beacon_type = esp_ble_is_mybeacon_packet (scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len, scan_result->scan_rst.scan_rsp_len);

            if( (beacon_type == BEACON_V3) || (beacon_type == BEACON_V4) || (beacon_type == BEACON_V4_SR) ){

                // ESP_LOGD(TAG, "mybeacon found, type %d", beacon_type);
                switch(beacon_type){

                case BEACON_V3: {
                    decode_mybeacon_packet_v3((esp_ble_mybeacon_v3_t*)(scan_result->scan_rst.ble_adv), &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active, &is_beacon_close);
                    break;
                }

                case BEACON_V4:
                case BEACON_V4_SR:
                {
                    esp_ble_mybeacon_payload_t *mybeacon_payload = (esp_ble_mybeacon_payload_t *)(&scan_result->scan_rst.ble_adv[11]);

                    decode_mybeacon_packet_v4(mybeacon_payload, scan_result->scan_rst.ble_adv, &idx, &maj, &min, &temp, &humidity, &battery,
                        &x, &y, &z, scan_result->scan_rst.rssi, &is_beacon_active, &is_beacon_close);
                    break;
                }
                default:
                    ESP_LOGE(TAG, "ESP_GAP_SEARCH_INQ_RES_EVT: should not happen");
                    break;
                }

                // ESP_LOGD(TAG, "(0x%04x%04x) rssi %3d, found, is_beacon_close %d, display_message_is_shown %d, idx %d",
                //     maj, min, scan_result->scan_rst.rssi, is_beacon_close, display_status.display_message_is_shown, idx);

                if(is_beacon_close){
                    if(display_status.display_message_is_shown){
                        oneshot_display_message_timer_touch();
                    } else {
                        display_message_content.beac = idx;
                        snprintf(display_message_content.title, 32, "Beacon Identified");
                        snprintf(display_message_content.message, 32, "Name: %s", ble_beacons[idx].beacon_data.name);
                        snprintf(display_message_content.comment, 32, "Activated: %c", (is_beacon_idx_active(idx)? 'y':'n'));
                        snprintf(display_message_content.action, 32, "%s", "Toggle active w/Button");
                        display_message_show();
                    }
                }

                if(!is_beacon_active){
                    break;
                }

#if CONFIG_USE_MQTT==1
                mqtt_send_adv = send_to_mqtt(idx, maj, min, temp, humidity, battery, scan_result->scan_rst.rssi);
#endif

                ESP_LOGI(TAG, "(0x%04x%04x) rssi %3d | temp %5.1f | hum %5.1f | x %+6d | y %+6d | z %+6d | batt %4d | mqtt send %c",
                    maj, min, scan_result->scan_rst.rssi, temp, humidity, x, y, z, battery, (mqtt_send_adv ? 'y':'n') );

                if(!is_beacon_bd_addr_set(maj, min)){
                    set_beaconaddress(maj, min, &scan_result->scan_rst.bda);
                }

                update_adv_data(maj, min, scan_result->scan_rst.rssi, temp, humidity, battery, mqtt_send_adv);
                check_update_display(maj, min);

                if ((ble_beacons[idx].offline_buffer_status == OFFLINE_BUFFER_STATUS_DOWNLOAD_REQUESTED) && (gattc_connect == false)) {
                    if(gattc_give_up_now == false){
                        gattc_connect = true;
                        gattc_connect_beacon_idx = idx;
                        ble_beacons[idx].offline_buffer_status = OFFLINE_BUFFER_STATUS_DOWNLOAD_IN_PROGRESS;
                        gattc_offline_buffer_downloading = true;

                        ESP_LOGD(TAG, "connect to the remote device.");
                        esp_ble_gap_stop_scanning();
                        // expose to client
                        // esp_ble_gattc_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if, scan_result->scan_rst.bda, scan_result->scan_rst.ble_addr_type, true);
                    } else {
                        free_offline_buffer(idx, OFFLINE_BUFFER_STATUS_NONE);
                        gattc_give_up_now = false;
                    }
                }
            } else {
                // ESP_LOGD(TAG, "mybeacon not found");
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            ESP_LOGD(TAG, "ESP_GAP_SEARCH_INQ_CMPL_EVT");
            break;
        default:
            ESP_LOGE(TAG, "ESP_GAP_BLE_SCAN_RESULT_EVT - default, evt %d", event);
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
        if ((err = param->scan_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Scan stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGD(TAG, "Stop scan successfully");
        }
        gattc_scanning = false;

        // if(web_file_server_running){
        //     ESP_LOGD(TAG, "temporarily stop the web server");
        //     stop_webserver(web_server);
        //     web_file_server_temporarily_stopped = true;
        // }

        break;
    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT");
        if ((err = param->adv_stop_cmpl.status) != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(TAG, "Adv stop failed: %s", esp_err_to_name(err));
        }
        else {
            ESP_LOGD(TAG, "Stop adv successfully");
        }
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGD(TAG, "ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT");
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                  param->update_conn_params.status,
                  param->update_conn_params.min_int,
                  param->update_conn_params.max_int,
                  param->update_conn_params.conn_int,
                  param->update_conn_params.latency,
                  param->update_conn_params.timeout);
        break;
    default:
        ESP_LOGE(TAG, "esp_gap_cb - default (%u)", event);
        break;
    }
}

void initialize_ble()
{
    remote_filter_service_uuid.len = ESP_UUID_LEN_128;
    memcpy(remote_filter_service_uuid.uuid.uuid128, REMOTE_SERVICE_UUID, 16);

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());
    ESP_ERROR_CHECK(esp_ble_gatts_register_callback(esp_gatts_cb));
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(esp_gap_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(esp_gattc_cb));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(PROFILE_ID_BEACON));
    ESP_ERROR_CHECK(esp_ble_gatts_app_register(PROFILE_ID_CTS));
    ESP_ERROR_CHECK(esp_ble_gatt_set_local_mtu(500));
}

void initialize_ble_security()
{
    /* set the security iocap & auth_req & key size & init key response key parameters to the stack*/
    esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;     //bonding with peer device after authentication
    esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;           //set the IO capability to No output No input
    uint8_t key_size = 16;      //the key size should be 7~16 bytes
    uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
    uint8_t oob_support = ESP_BLE_OOB_DISABLE;
    esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_OOB_SUPPORT, &oob_support, sizeof(uint8_t));
    /* If your BLE device act as a Slave, the init_key means you hope which types of key of the master should distribute to you,
    and the response key means which key you can distribute to the Master;
    If your BLE device act as a master, the response key means you hope which types of key of the slave should distribute to you,
    and the init key means which key you can distribute to the slave. */
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
    esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
}
