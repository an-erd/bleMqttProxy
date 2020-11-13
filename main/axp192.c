#include "blemqttproxy.h"
#include "driver/i2c.h"
#include "esp_log.h"

#include "axp192.h"

static const char* TAG = "AXP192";

#define AXP192_I2C_ADDRESS                      0x34
#define AXP192_BATTERYLEVEL                     0x78


static uint16_t axp192_read_12bit(uint8_t addr)
{
	esp_err_t ret;
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    uint8_t data[2];

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (AXP192_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, I2C_MASTER_ACK);
	i2c_master_write_byte(cmd, addr, I2C_MASTER_ACK);
	i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "axp192_read_12bit write failed. code: 0x%.2X", ret);
	}
    i2c_cmd_link_delete(cmd);

    cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (AXP192_I2C_ADDRESS << 1) | I2C_MASTER_READ, I2C_MASTER_ACK);
    i2c_master_read(cmd, data, 2, I2C_MASTER_LAST_NACK);
    i2c_master_stop(cmd);
    ret = i2c_master_cmd_begin(I2C_NUM_0, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "axp192_read_12bit read failed. code: 0x%.2X", ret);
	}

    return (uint16_t) (data[0] << 4) | data[1];
}

float axp192_get_battery_voltage()
{
    float ADCLSB = 1.1 / 1000.0;
    uint16_t ReData = axp192_read_12bit(AXP192_BATTERYLEVEL);

    return ReData * ADCLSB;
}