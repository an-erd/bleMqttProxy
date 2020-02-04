#ifndef __BEACON_H__
#define __BEACON_H__

typedef struct  {
    uint8_t     proximity_uuid[4];
    uint16_t    major;
    uint16_t    minor;
    char        name[8];
} ble_beacon_data_t;

typedef struct  {
    int8_t      measured_power;
    float       temp;
    float       humidity;
    uint16_t    battery;
    int64_t     last_seen;
    int64_t     mqtt_last_send;
} ble_adv_data_t;

extern ble_beacon_data_t ble_beacon_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED];
extern ble_adv_data_t ble_adv_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED];
extern uint16_t s_active_beacon_mask;
extern char ble_beacon_names_seen[CONFIG_BLE_DEVICE_COUNT_CONFIGURED][8];

// Beacon
typedef enum {
    BEACON_V3           = 0,    // ble_bacon v3 adv
    BEACON_V4,                  // only ble_beacon v4 adv
    BEACON_V4_SR,               // ble_beacon v4 adv+sr
    UNKNOWN_BEACON      = 99
} beacon_type_t;

typedef struct {
    uint8_t flags[3];
    uint8_t length;
    uint8_t type;
    uint16_t company_id;
    uint16_t beacon_type;
}__attribute__((packed)) esp_ble_mybeacon_head_t;

typedef struct {
    uint8_t  proximity_uuid[4];
    uint16_t major;
    uint16_t minor;
    int8_t   measured_power;
}__attribute__((packed)) esp_ble_mybeacon_vendor_t;

typedef struct {
    uint16_t temp;
    uint16_t humidity;
    uint16_t x;
    uint16_t y;
    uint16_t z;
    uint16_t battery;
}__attribute__((packed)) esp_ble_mybeacon_payload_t;

typedef struct {
    esp_ble_mybeacon_head_t     mybeacon_head;
    esp_ble_mybeacon_vendor_t   mybeacon_vendor;
    esp_ble_mybeacon_payload_t  mybeacon_payload;
}__attribute__((packed)) esp_ble_mybeacon_v3_t;

extern esp_ble_mybeacon_head_t mybeacon_common_head_v3;
extern esp_ble_mybeacon_head_t mybeacon_common_head_v4;
extern esp_ble_mybeacon_vendor_t mybeacon_common_vendor_v3;

beacon_type_t esp_ble_is_mybeacon_packet (uint8_t *adv_data, uint8_t adv_data_len, uint8_t scan_rsp_len);
void decode_mybeacon_packet_v3(esp_ble_mybeacon_v3_t *mybeacon_data,
    uint8_t *idx, uint16_t *maj, uint16_t *min, float *temp, float *humidity, uint16_t *battery,
    int16_t *x, int16_t *y, int16_t *z, int8_t rssi, bool *is_beacon_active);
void decode_mybeacon_packet_v4(esp_ble_mybeacon_payload_t *mybeacon_payload, uint8_t *ble_adv,
    uint8_t *idx, uint16_t *maj, uint16_t *min, float *temp, float *humidity, uint16_t *battery,
    int16_t *x, int16_t *y, int16_t *z, int8_t rssi, bool *is_beacon_active);

uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min);
uint8_t num_active_beacon();
uint8_t first_active_beacon();
bool is_beacon_idx_active(uint16_t idx);
void set_beacon_idx_active(uint16_t idx);
void clear_beacon_idx_active(uint16_t idx);
bool toggle_beacon_idx_active(uint16_t idx);
void persist_active_beacon_mask();

uint8_t num_beacon_name_known();
uint8_t is_beacon_name_known(char* adv_name);       // returns 0 if unknown, otherwise index in ble_beacon_names_seen[]
uint8_t add_known_beacon_name(char* adv_name);
char* get_known_beacon_name(uint8_t idx);

#endif // __BEACON_H__
