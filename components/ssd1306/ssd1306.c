// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_log.h"
#include "driver/i2c.h"
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <time.h>
#include <sys/time.h>
#include <string.h>

static const char* TAG = "SSD1306";

static uint32_t _pow(uint8_t m, uint8_t n)
{
    uint32_t result = 1;
    while (n--) {
        result *= m;
    }
    return result;
}

ssd1306_canvas_t *create_ssd1306_canvas(uint8_t width, uint8_t height, uint8_t x_orig, uint8_t y_orig, uint8_t chFill){
    ssd1306_canvas_t *tmp_canvas = (ssd1306_canvas_t *) calloc(1, sizeof(ssd1306_canvas_t));
    tmp_canvas->w        = width;
    tmp_canvas->h        = height;
    tmp_canvas->x_origin = x_orig;
    tmp_canvas->y_origin = y_orig;
    tmp_canvas->s_chDisplayBuffer = (uint8_t *) calloc(tmp_canvas->w * tmp_canvas->h, sizeof(uint8_t));
    memset(tmp_canvas->s_chDisplayBuffer, chFill, tmp_canvas->w * tmp_canvas->h);

    return tmp_canvas;
}

void delete_ssd1306_canvas(ssd1306_canvas_t *canvas){
    free(canvas->s_chDisplayBuffer);
    free(canvas);
}

void i2c_master_init()
{
	i2c_config_t i2c_config = {
		.mode               = I2C_MODE_MASTER,
		.sda_io_num         = SDA_PIN,
		.scl_io_num         = SCL_PIN,
		.sda_pullup_en      = GPIO_PULLUP_ENABLE,
		.scl_pullup_en      = GPIO_PULLUP_ENABLE,
		.master.clk_speed   = OLED_IIC_FREQ_HZ
	};
	i2c_param_config(I2C_NUM_0, &i2c_config);
	i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0);
}

void ssd1306_init() {
	esp_err_t espRc;

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();

	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);
	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);

	i2c_master_write_byte(cmd, OLED_CMD_SET_CHARGE_PUMP, true);
	i2c_master_write_byte(cmd, 0x14, true);
    i2c_master_write_byte(cmd, OLED_CMD_SET_SEGMENT_REMAP, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_COM_SCAN_MODE_REMAP, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_CONTRAST, true);
	i2c_master_write_byte(cmd, 0xFF, true);

	i2c_master_write_byte(cmd, OLED_CMD_DISPLAY_ON, true);
	i2c_master_stop(cmd);

	espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	if (espRc == ESP_OK) {
		ESP_LOGI(TAG, "OLED configured successfully");
	} else {
		ESP_LOGE(TAG, "OLED configuration failed. code: 0x%.2X", espRc);
	}
	i2c_cmd_link_delete(cmd);
    // iot_ssd1306_refresh_gram(dev);
}

void ssd1306_fill_point(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        uint8_t chPoint)
{
    uint8_t chPos, chBx, chTemp = 0;

    if (chXpos > canvas->w || chYpos > (canvas->h * OLED_PIXEL_PER_PAGE)) {
        return;
    }
    chPos = chYpos / 8;
    chBx = chYpos % 8;
    chTemp = 1 << chBx;

    if (chPoint) {
        canvas->s_chDisplayBuffer[chPos * canvas->w + chXpos] |= chTemp;
    } else {
        canvas->s_chDisplayBuffer[chPos * canvas->w + chXpos] &= ~chTemp;
    }
}

esp_err_t ssd1306_fill_rectangle(ssd1306_canvas_t *canvas, uint8_t chXpos1, uint8_t chYpos1,
    uint8_t chXpos2, uint8_t chYpos2, uint8_t chDot)
{
    uint8_t chXpos, chYpos;
    uint8_t y1Page, y2Page;
    uint8_t fillpattern;

    y1Page = chYpos1       / OLED_PIXEL_PER_PAGE + (chYpos1 % OLED_PIXEL_PER_PAGE ? 1 : 0 );
    y2Page = (chYpos2 + 1) / OLED_PIXEL_PER_PAGE;

ESP_LOGI(TAG, "ssd1306_fill_rectangle: y1 %d y2 %d y1Page %d y2Page %d", chYpos1, chYpos2, y1Page, y2Page);
    // middle block (y-wise full pages)
    for (int i = y1Page; i < y2Page; i++){
        ESP_LOGI(TAG, "ssd1306_fill_rectangle: i %d chXpos1 %d chXpos2 %d, chXpos2 - chXpos1 + 1 %d", i, chXpos1, chXpos2, (chXpos2 - chXpos1 + 1));
        memset(&canvas->s_chDisplayBuffer[i * canvas->w + chXpos1], (chDot ? 0xFF : 0x00), chXpos2 - chXpos1 + 1);
    }

    // top block (not a full page)
    if(chYpos1 % OLED_PIXEL_PER_PAGE){
        fillpattern = 0xFF << (chYpos1 % OLED_PIXEL_PER_PAGE);  // 1: fill, 0: don't overwrite
        if(chDot){
            for (chXpos = chXpos1; chXpos <= chXpos2; chXpos++) {
                canvas->s_chDisplayBuffer[(y1Page - 1) * canvas->w  + chXpos] |= fillpattern;
            }
        } else {
            for (chXpos = chXpos1; chXpos <= chXpos2; chXpos++) {
                canvas->s_chDisplayBuffer[(y1Page - 1) * canvas->w  + chXpos] &= ~fillpattern;
            }
        }
    }

    // bottom block (not a full page)
    if( (chYpos2 + 1) % OLED_PIXEL_PER_PAGE){
        fillpattern = 0xFF >> ( (chYpos2 + 1) % OLED_PIXEL_PER_PAGE);
        if(chDot){
            for (chXpos = chXpos1; chXpos <= chXpos2; chXpos++) {
                canvas->s_chDisplayBuffer[y2Page * canvas->w + chXpos] |= fillpattern;
            }
        } else {
            for (chXpos = chXpos1; chXpos <= chXpos2; chXpos++) {
                canvas->s_chDisplayBuffer[y2Page * canvas->w + chXpos] &= ~fillpattern;
            }
        }
    }

    return ESP_OK;
}

void ssd1306_draw_char(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChr, uint8_t chSize, uint8_t chMode)
{
    uint8_t i, j;
    uint8_t chTemp, chYpos0 = chYpos;

    chChr = chChr - ' ';
    for (i = 0; i < chSize; i++) {
        if (chSize == 12) {
            if (chMode) {
                chTemp = c_chFont1206[chChr][i];
            } else {
                chTemp = ~c_chFont1206[chChr][i];
            }
        } else if (chSize == 10) {
            if (chMode) {
                chTemp = c_chFont1006[chChr][i];
            } else {
                chTemp = ~c_chFont1006[chChr][i];
            }
        } else {
            if (chMode) {
                chTemp = c_chFont1608[chChr][i];
            } else {
                chTemp = ~c_chFont1608[chChr][i];
            }
        }

        for (j = 0; j < 8; j++) {
            if (chTemp & 0x80) {
                ssd1306_fill_point(canvas, chXpos, chYpos, 1);
            } else {
                ssd1306_fill_point(canvas, chXpos, chYpos, 0);
            }
            chTemp <<= 1;
            chYpos++;

            if ((chYpos - chYpos0) == chSize) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void ssd1306_draw_num(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        uint32_t chNum, uint8_t chLen, uint8_t chSize)
{
    uint8_t i;
    uint8_t chTemp, chShow = 0;

    for (i = 0; i < chLen; i++) {
        chTemp = (chNum / _pow(10, chLen - i - 1)) % 10;
        if (chShow == 0 && i < (chLen - 1)) {
            if (chTemp == 0) {
                ssd1306_draw_char(canvas, chXpos + (chSize / 2) * i, chYpos,
                        ' ', chSize, 1);
                continue;
            } else {
                chShow = 1;
            }
        }
        ssd1306_draw_char(canvas, chXpos + (chSize / 2) * i, chYpos,
                chTemp + '0', chSize, 1);
    }
}

void ssd1306_draw_1616char(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    uint8_t i, j;
    uint8_t chTemp = 0, chYpos0 = chYpos, chMode = 0;

    for (i = 0; i < 32; i++) {
        chTemp = c_chFont1612[chChar - 0x30][i];
        for (j = 0; j < 8; j++) {
            chMode = chTemp & 0x80 ? 1 : 0;
            ssd1306_fill_point(canvas, chXpos, chYpos, chMode);
            chTemp <<= 1;
            chYpos++;
            if ((chYpos - chYpos0) == 16) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void ssd1306_draw_3216char(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        uint8_t chChar)
{
    uint8_t i, j;
    uint8_t chTemp = 0, chYpos0 = chYpos, chMode = 0;

    for (i = 0; i < 64; i++) {
        chTemp = c_chFont3216[chChar - 0x30][i];
        for (j = 0; j < 8; j++) {
            chMode = chTemp & 0x80 ? 1 : 0;
            ssd1306_fill_point(canvas, chXpos, chYpos, chMode);
            chTemp <<= 1;
            chYpos++;
            if ((chYpos - chYpos0) == 32) {
                chYpos = chYpos0;
                chXpos++;
                break;
            }
        }
    }
}

void ssd1306_draw_bitmap(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos,
        const uint8_t *pchBmp, uint8_t chWidth, uint8_t chHeight)
{
    uint16_t i, j, byteWidth = (chWidth + 7) / 8;

    for (j = 0; j < chHeight; j++) {
        for (i = 0; i < chWidth; i++) {
            if (*(pchBmp + j * byteWidth + i / 8) & (128 >> (i & 7))) {
                ssd1306_fill_point(canvas, chXpos + i, chYpos + j, 1);
            }
        }
    }
}

esp_err_t ssd1306_draw_string(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, const uint8_t *pchString, uint8_t chSize,
        uint8_t chMode)
{
    esp_err_t ret = ESP_OK;
    while (*pchString != '\0') {
        if (chXpos > (OLED_WIDTH - chSize / 2)) {
            chXpos = 0;
            chYpos += chSize;
            if (chYpos > (OLED_HEIGHT - chSize)) {
                chYpos = chXpos = 0;
                ssd1306_clear_canvas(canvas, 0x00);
                if (ret == ESP_FAIL) {
                    return ret;
                }
            }
        }
        ssd1306_draw_char(canvas, chXpos, chYpos, *pchString, chSize, chMode);
        chXpos += chSize / 2;
        pchString++;
    }
    return ret;
}

void ssd1306_draw_string_8x8(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chPage, const uint8_t *pchString)
{
    uint8_t *pCol = (uint8_t *) &canvas->s_chDisplayBuffer[chPage * canvas->w + chXpos];
    uint8_t cols_used = 0;
    uint8_t cols_avail = canvas->w - chXpos;

    while (*pchString != '\0') {
        memcpy(pCol, c_font8x8_basic_tr[*pchString], (cols_used + 8 <= cols_avail ? 8 : cols_avail - cols_used));
        pchString++;    // next char in source
        pCol += 8;   // pCol to target starting column for next char
        cols_used += 8;
        if(cols_used >= cols_avail)
            break;
    }
}

esp_err_t ssd1306_refresh_gram(ssd1306_canvas_t *canvas)
{
   	i2c_cmd_handle_t cmd;

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_CMD_STREAM, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_MEMORY_ADDR_MODE, true);
	i2c_master_write_byte(cmd, 0x00, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_COLUMN_RANGE, true);
	i2c_master_write_byte(cmd, canvas->x_origin, true);
	i2c_master_write_byte(cmd, canvas->x_origin + canvas->w - 1, true);
	i2c_master_write_byte(cmd, OLED_CMD_SET_PAGE_RANGE, true);
	i2c_master_write_byte(cmd, canvas->y_origin, true);
	i2c_master_write_byte(cmd, canvas->y_origin + canvas->h - 1, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

	cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, (OLED_I2C_ADDRESS << 1) | I2C_MASTER_WRITE, true);

	i2c_master_write_byte(cmd, OLED_CONTROL_BYTE_DATA_STREAM, true);
	i2c_master_write(cmd, (uint8_t *)canvas->s_chDisplayBuffer, canvas->w * canvas->h, true);

	i2c_master_stop(cmd);
	i2c_master_cmd_begin(I2C_NUM_0, cmd, 10/portTICK_PERIOD_MS);
	i2c_cmd_link_delete(cmd);

    return ESP_OK;  // TODO
}

void ssd1306_clear_canvas(ssd1306_canvas_t *canvas, uint8_t chFill)
{
    memset(canvas->s_chDisplayBuffer, chFill, canvas->w * canvas->h);
}

