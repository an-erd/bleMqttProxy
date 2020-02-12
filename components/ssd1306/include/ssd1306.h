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
#ifndef __SSD_1306_H__
#define __SSD_1306_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "stdint.h"

typedef struct {
    uint8_t* s_chDisplayBuffer; // addressing: [page][column]
    uint8_t w;                  // width in pixel
    uint8_t h;                  // height in pages, i.e. *8 for pixel
    uint8_t x_origin;           // column in SSD1306 GRAM
    uint8_t y_origin;           // page in SSD1306 GRAM
} ssd1306_canvas_t;


#define SDA_PIN GPIO_NUM_5
#define SCL_PIN GPIO_NUM_4

// Code from https://github.com/yanbe/ssd1306-esp-idf-i2c.git is used as a starting point,
// in addition to code from https://github.com/espressif/esp-iot-solution.
// Following definitions are borrowed from
// http://robotcantalk.blogspot.com/2015/03/interfacing-arduino-with-ssd1306-driven.html

// SLA (0x3C) + WRITE_MODE (0x00) =  0x78 (0b01111000)
#define OLED_I2C_ADDRESS                    0x3C
#define OLED_WIDTH                          128
#define OLED_HEIGHT                         64
#define OLED_COLUMNS                        128
#define OLED_PAGES                          8
#define OLED_PIXEL_PER_PAGE                 8

// Control byte
#define OLED_CONTROL_BYTE_CMD_SINGLE        0x80
#define OLED_CONTROL_BYTE_CMD_STREAM        0x00
#define OLED_CONTROL_BYTE_DATA_STREAM       0x40

// Fundamental commands (pg.28)
#define OLED_CMD_SET_CONTRAST               0x81    // follow with 0x7F
#define OLED_CMD_DISPLAY_RAM                0xA4
#define OLED_CMD_DISPLAY_ALLON              0xA5
#define OLED_CMD_DISPLAY_NORMAL             0xA6
#define OLED_CMD_DISPLAY_INVERTED           0xA7
#define OLED_CMD_DISPLAY_OFF                0xAE
#define OLED_CMD_DISPLAY_ON                 0xAF

// Addressing Command Table (pg.30)
#define OLED_CMD_SET_MEMORY_ADDR_MODE       0x20    // follow with 0x00 = HORZ mode
#define OLED_CMD_SET_COLUMN_RANGE           0x21    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x7F = COL127
#define OLED_CMD_SET_PAGE_RANGE             0x22    // can be used only in HORZ/VERT mode - follow with 0x00 and 0x07 = PAGE7

// Hardware Config (pg.31)
#define OLED_CMD_SET_DISPLAY_START_LINE     0x40
#define OLED_CMD_SET_SEGMENT_REMAP          0xA1
#define OLED_CMD_SET_MUX_RATIO              0xA8    // follow with 0x3F = 64 MUX
#define OLED_CMD_SET_COM_SCAN_MODE_NORMAL   0xC0
#define OLED_CMD_SET_COM_SCAN_MODE_REMAP    0xC8
#define OLED_CMD_SET_DISPLAY_OFFSET         0xD3    // follow with 0x00
#define OLED_CMD_SET_COM_PIN_MAP            0xDA    // follow with 0x12
#define OLED_CMD_NOP                        0xE3    // NOP

// Timing and Driving Scheme (pg.32)
#define OLED_CMD_SET_DISPLAY_CLK_DIV        0xD5    // follow with 0x80
#define OLED_CMD_SET_PRECHARGE              0xD9    // follow with 0xF1
#define OLED_CMD_SET_VCOMH_DESELCT          0xDB    // follow with 0x30

// Charge Pump (pg.62)
#define OLED_CMD_SET_CHARGE_PUMP            0x8D    // follow with 0x14

#define OLED_IIC_FREQ_HZ                    400000  // I2C colock frequency

/**
 * @brief   canvas object initialization
 *
 * @param   width Specifies the with in columns (each 1 pixel)
 * @param   height Specifies the with in pages (each 8 pixel)
 * @param   x_orig Specifies the x position in actual display columns (each 1 pixel)
 * @param   y_orig Specifies the y position in actual display pages (each 8 pixel)
 * @param   chFill Specifies the char to initialize the canvas with
 *
 * @return
 *     - pointer to the created canvas object
 */
ssd1306_canvas_t *create_ssd1306_canvas(uint8_t width, uint8_t height, uint8_t x_orig,
        uint8_t y_orig, uint8_t chFill);

/**
 * @brief   canvas object deletion
 *
 * @param   canvas Specifies the canvas object to delete
 *
 * @return
 *     - none
 */
void delete_ssd1306_canvas(ssd1306_canvas_t *canvas);


/**
 * @brief   i2c master initialization
 *
 * @param   none
 *
 * @return
 *     - none
 */
void i2c_master_init();

/**
 * @brief   ssd1306 device initialization
 *
 * @param   none
 *
 * @return
 *     -
 *
 */
void ssd1306_init();

/**
 * @brief   ssd1306 device display on
 *
 * @param   none
 *
 * @return
 *     -
 *
 */
void ssd1306_display_on();

/**
 * @brief   ssd1306 device display off
 *
 * @param   none
 *
 * @return
 *     -
 *
 */
void ssd1306_display_off();



/**
 * @brief   draw point on (x, y)
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   chPoint fill point
 *
 * @return
 *     - NULL
 */
void ssd1306_fill_point(ssd1306_canvas_t *canvas, uint8_t chXpos, uint8_t chYpos, uint8_t chPoint);

/**
 * @brief   Draw rectangle on (x1,y1)-(x2,y2)
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos1
 * @param   chYpos1
 * @param   chXpos2
 * @param   chYpos2
 * @param   chDot fill point
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t ssd1306_fill_rectangle(ssd1306_canvas_t *canvas, uint8_t chXpos1, uint8_t chYpos1,
        uint8_t chXpos2, uint8_t chYpos2, uint8_t chDot);

/**
 * @brief   display char on (x, y),and set size, mode
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   chSize char size
 * @param   chChr draw char
 * @param   chMode display mode
 *
 * @return
 *     - NULL
 */
void ssd1306_draw_char(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, uint8_t chChr, uint8_t chSize, uint8_t chMode);

/**
 * @brief   display number on (x, y),and set length, size, mode
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   chNum draw num
 * @param   chLen length
 * @param   chSize display size
 *
 * @return
 *     - NULL
 */
void ssd1306_draw_num(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, uint32_t chNum, uint8_t chLen, uint8_t chSize);

/**
 * @brief   display 1616char on (x, y)
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   chChar draw char
 *
 * @return
 *     - NULL
 */
void ssd1306_draw_1616char(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, uint8_t chChar);

/**
 * @brief   display 3216char on (x, y)
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   chChar draw char
 *
 * @return
 *     - NULL
 */
void ssd1306_draw_3216char(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, uint8_t chChar);

/**
 * @brief   draw bitmap on (x, y),and set width, height
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   pchBmp point to BMP data
 * @param   chWidth picture width
 * @param   chHeight picture heght
 *
 * @return
 *     - NULL
 */
void ssd1306_draw_bitmap(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, const uint8_t *pchBmp, uint8_t chWidth,
        uint8_t chHeight);

/**
 * @brief   Displays a string on the screen
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the Y position
 * @param   pchString Pointer to a string to display on the screen
 * @param   chSize char size
 * @param   chMode display mode
 *
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 **/
esp_err_t ssd1306_draw_string(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chYpos, const uint8_t *pchString, uint8_t chSize,
        uint8_t chMode);

/**
 * @brief   Displays a string on the screen
 *
 * @param   canvas Specifies the display canvas object
 * @param   chXpos Specifies the X position
 * @param   chYpos Specifies the page (=line) position
 * @param   pchString Pointer to a string to display on the screen
 *
 * @return
 *     -
 *
 **/
void ssd1306_draw_string_8x8(ssd1306_canvas_t *canvas, uint8_t chXpos,
        uint8_t chPage, const uint8_t *pchString);

/**
 * @brief   refresh dot matrix panel
 *
 * @param   canvas Specifies the display canvas object

 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 **/
esp_err_t ssd1306_refresh_gram(ssd1306_canvas_t *canvas);

/**
 * @brief   Clear screen
 *
 * @param   canvas Specifies the display canvas object
 * @param   chFill whether fill and fill char
 *
 * @return
 *     -
 *
 **/
void ssd1306_clear_canvas(ssd1306_canvas_t *canvas, uint8_t chFill);

#ifdef __cplusplus
}
#endif

#endif
