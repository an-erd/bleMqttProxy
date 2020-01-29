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

ble_beacon_data_t ble_beacon_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] = {
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_1_MAJ,  CONFIG_BLE_DEVICE_1_MIN,  CONFIG_BLE_DEVICE_1_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_2_MAJ,  CONFIG_BLE_DEVICE_2_MIN,  CONFIG_BLE_DEVICE_2_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_3_MAJ,  CONFIG_BLE_DEVICE_3_MIN,  CONFIG_BLE_DEVICE_3_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_4_MAJ,  CONFIG_BLE_DEVICE_4_MIN,  CONFIG_BLE_DEVICE_4_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_5_MAJ,  CONFIG_BLE_DEVICE_5_MIN,  CONFIG_BLE_DEVICE_5_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_6_MAJ,  CONFIG_BLE_DEVICE_6_MIN,  CONFIG_BLE_DEVICE_6_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_7_MAJ,  CONFIG_BLE_DEVICE_7_MIN,  CONFIG_BLE_DEVICE_7_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_8_MAJ,  CONFIG_BLE_DEVICE_8_MIN,  CONFIG_BLE_DEVICE_8_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_9_MAJ,  CONFIG_BLE_DEVICE_9_MIN,  CONFIG_BLE_DEVICE_9_NAME},
    { {CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4}, CONFIG_BLE_DEVICE_10_MAJ, CONFIG_BLE_DEVICE_10_MIN, CONFIG_BLE_DEVICE_10_NAME},
};
ble_adv_data_t    ble_adv_data[CONFIG_BLE_DEVICE_COUNT_CONFIGURED] = { 0 };

extern uint16_t s_active_beacon_mask;

// Beacon
typedef enum {
    BEACON_V3           = 0,    // ble_bacon v3 adv
    BEACON_V4,                  // only ble_beacon v4 adv
    BEACON_V4_SR,                // ble_beacon v4 adv+sr
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

esp_ble_mybeacon_head_t mybeacon_common_head_v3 = {
    .flags = {0x02, 0x01, 0x04},
    .length = 0x1A,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x1502
};

esp_ble_mybeacon_head_t mybeacon_common_head_v4 = {
    .flags = {0x02, 0x01, 0x06},
    .length = 0x13,
    .type = 0xFF,
    .company_id = 0x0059,
    .beacon_type = 0x0700
};

esp_ble_mybeacon_vendor_t mybeacon_common_vendor_v3 = {
    .proximity_uuid ={CONFIG_BLE_UUID_1, CONFIG_BLE_UUID_2, CONFIG_BLE_UUID_3, CONFIG_BLE_UUID_4},
};


static uint8_t beacon_maj_min_to_idx(uint16_t maj, uint16_t min);
static uint8_t num_active_beacon();
static uint8_t first_active_beacon();
static bool is_beacon_idx_active(uint16_t idx);
static void set_beacon_idx_active(uint16_t idx);
static void clear_beacon_idx_active(uint16_t idx);
static bool toggle_beacon_idx_active(uint16_t idx);
static void persist_active_beacon_mask();


#endif // __BEACON_H__
