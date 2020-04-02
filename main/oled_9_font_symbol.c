#include "lvgl/lvgl.h"

/*******************************************************************************
 * Size: 9 px
 * Bpp: 1
 * Opts: 
 ******************************************************************************/

#ifndef OLED_9_FONT_SYMBOL
#define OLED_9_FONT_SYMBOL 1
#endif

#if OLED_9_FONT_SYMBOL

/*-----------------
 *    BITMAPS
 *----------------*/

/*Store the image of the glyphs*/
static LV_ATTRIBUTE_LARGE_CONST const uint8_t gylph_bitmap[] = {
    /* U+20 " " */
    0x0,

    /* U+21 "!" */
    0xfa,

    /* U+22 "\"" */
    0xf0,

    /* U+23 "#" */
    0x29, 0x9e, 0xaf, 0xa9, 0x40,

    /* U+24 "$" */
    0x25, 0x96, 0x31, 0x96,

    /* U+25 "%" */
    0xc3, 0xd, 0x38, 0x39, 0x51, 0x80,

    /* U+26 "&" */
    0x64, 0x98, 0x8a, 0xc9, 0xe0,

    /* U+27 "'" */
    0xc0,

    /* U+28 "(" */
    0x4a, 0x49, 0x22, 0x40,

    /* U+29 ")" */
    0x48, 0x92, 0x4a, 0x80,

    /* U+2A "*" */
    0x27, 0x25,

    /* U+2B "+" */
    0x21, 0x3e, 0x42, 0x0,

    /* U+2C "," */
    0xc0,

    /* U+2D "-" */
    0xc0,

    /* U+2E "." */
    0x80,

    /* U+2F "/" */
    0x24, 0xa5, 0x20,

    /* U+30 "0" */
    0x69, 0x99, 0x99, 0x60,

    /* U+31 "1" */
    0x3c, 0x92, 0x48,

    /* U+32 "2" */
    0x69, 0x12, 0x48, 0xf0,

    /* U+33 "3" */
    0x69, 0x16, 0x19, 0xe0,

    /* U+34 "4" */
    0x11, 0x94, 0xa9, 0x7c, 0x40,

    /* U+35 "5" */
    0xf8, 0xe1, 0x19, 0x60,

    /* U+36 "6" */
    0x6c, 0xe9, 0x99, 0x60,

    /* U+37 "7" */
    0xf1, 0x22, 0x44, 0x40,

    /* U+38 "8" */
    0x6d, 0xd6, 0x99, 0x60,

    /* U+39 "9" */
    0x69, 0x99, 0x73, 0x60,

    /* U+3A ":" */
    0x88,

    /* U+3B ";" */
    0xc0, 0x50,

    /* U+3C "<" */
    0x1e, 0xc3,

    /* U+3D "=" */
    0xe3, 0x80,

    /* U+3E ">" */
    0x86, 0x3c,

    /* U+3F "?" */
    0x79, 0x12, 0x40, 0x40,

    /* U+40 "@" */
    0x38, 0x8a, 0x6d, 0x5c, 0xb9, 0x6f, 0x20, 0x30,

    /* U+41 "A" */
    0x20, 0xc5, 0x12, 0x7a, 0x28, 0x40,

    /* U+42 "B" */
    0xe4, 0xa5, 0xc9, 0x4b, 0xc0,

    /* U+43 "C" */
    0x76, 0x61, 0x8, 0x65, 0xc0,

    /* U+44 "D" */
    0xf4, 0xe3, 0x18, 0xcf, 0xc0,

    /* U+45 "E" */
    0xf8, 0x8f, 0x88, 0xf0,

    /* U+46 "F" */
    0xf8, 0x8f, 0x88, 0x80,

    /* U+47 "G" */
    0x76, 0x61, 0x38, 0xe5, 0xc0,

    /* U+48 "H" */
    0x8c, 0x63, 0xf8, 0xc6, 0x20,

    /* U+49 "I" */
    0xfe,

    /* U+4A "J" */
    0x11, 0x11, 0x19, 0xe0,

    /* U+4B "K" */
    0x94, 0xa9, 0xca, 0x4a, 0x20,

    /* U+4C "L" */
    0x88, 0x88, 0x88, 0xf0,

    /* U+4D "M" */
    0x87, 0x3c, 0xf5, 0xb6, 0xda, 0x40,

    /* U+4E "N" */
    0x8e, 0x73, 0x5a, 0xce, 0x20,

    /* U+4F "O" */
    0x76, 0xe3, 0x18, 0xed, 0xc0,

    /* U+50 "P" */
    0xf4, 0x63, 0x1f, 0x42, 0x0,

    /* U+51 "Q" */
    0x76, 0xe3, 0x18, 0xed, 0xc1,

    /* U+52 "R" */
    0xe9, 0x9e, 0xa9, 0x90,

    /* U+53 "S" */
    0x74, 0x60, 0xc1, 0xc5, 0xc0,

    /* U+54 "T" */
    0xf9, 0x8, 0x42, 0x10, 0x80,

    /* U+55 "U" */
    0x8c, 0x63, 0x18, 0xc5, 0xc0,

    /* U+56 "V" */
    0x89, 0x24, 0x94, 0x30, 0xc2, 0x0,

    /* U+57 "W" */
    0x89, 0x9a, 0x5a, 0x5a, 0x66, 0x66, 0x24,

    /* U+58 "X" */
    0x4a, 0x8c, 0x43, 0x25, 0x20,

    /* U+59 "Y" */
    0x8a, 0x94, 0x42, 0x10, 0x80,

    /* U+5A "Z" */
    0xf8, 0x88, 0x44, 0x43, 0xe0,

    /* U+5B "[" */
    0xea, 0xaa, 0xc0,

    /* U+5C "\\" */
    0x84, 0x44, 0x22, 0x20,

    /* U+5D "]" */
    0xd5, 0x55, 0xc0,

    /* U+5E "^" */
    0xd, 0xd0,

    /* U+5F "_" */
    0xf0,

    /* U+60 "`" */
    0x90,

    /* U+61 "a" */
    0x79, 0xf9, 0xf0,

    /* U+62 "b" */
    0x88, 0xe9, 0x99, 0xe0,

    /* U+63 "c" */
    0x79, 0x89, 0x70,

    /* U+64 "d" */
    0x11, 0x79, 0x99, 0x70,

    /* U+65 "e" */
    0x69, 0xf8, 0x60,

    /* U+66 "f" */
    0x6b, 0xa4, 0x90,

    /* U+67 "g" */
    0x79, 0x99, 0x71, 0x60,

    /* U+68 "h" */
    0x88, 0xf9, 0x99, 0x90,

    /* U+69 "i" */
    0xbe,

    /* U+6A "j" */
    0x45, 0x55, 0xc0,

    /* U+6B "k" */
    0x88, 0xaa, 0xca, 0x90,

    /* U+6C "l" */
    0xfe,

    /* U+6D "m" */
    0xef, 0x26, 0x4c, 0x99, 0x20,

    /* U+6E "n" */
    0xf9, 0x99, 0x90,

    /* U+6F "o" */
    0x69, 0x99, 0x60,

    /* U+70 "p" */
    0xe9, 0x99, 0xe8, 0x80,

    /* U+71 "q" */
    0x79, 0x99, 0x71, 0x10,

    /* U+72 "r" */
    0xea, 0x80,

    /* U+73 "s" */
    0xe8, 0x4a, 0xe0,

    /* U+74 "t" */
    0x5d, 0x24, 0xc0,

    /* U+75 "u" */
    0x99, 0x99, 0xf0,

    /* U+76 "v" */
    0x99, 0x66, 0x20,

    /* U+77 "w" */
    0x95, 0x29, 0xd3, 0x64, 0x80,

    /* U+78 "x" */
    0x96, 0x26, 0x90,

    /* U+79 "y" */
    0x99, 0x66, 0x24, 0xc0,

    /* U+7A "z" */
    0xf2, 0x48, 0xf0,

    /* U+7B "{" */
    0x29, 0x28, 0x92, 0x20,

    /* U+7C "|" */
    0xff,

    /* U+7D "}" */
    0x89, 0x22, 0x92, 0x80,

    /* U+7E "~" */
    0xc9, 0x80,

    /* U+B0 "°" */
    0xbc,

    /* U+F001 "" */
    0x1, 0x87, 0xcf, 0xe7, 0x12, 0x9, 0x4, 0x8f,
    0xc7, 0xe0, 0x0,

    /* U+F008 "" */
    0xbf, 0x78, 0x7e, 0x1e, 0xfd, 0xe1, 0xe8, 0x5f,
    0xfc,

    /* U+F00B "" */
    0xef, 0xf7, 0xfb, 0xfd, 0xfe, 0xff, 0x7f, 0xbe,

    /* U+F00C "" */
    0x1, 0x1, 0xc1, 0xd9, 0xc7, 0xc1, 0xc0, 0x40,

    /* U+F00D "" */
    0xcf, 0xf7, 0x9e, 0xff, 0x30,

    /* U+F011 "" */
    0x8, 0x24, 0x92, 0x51, 0x18, 0x8c, 0x6, 0x2,
    0xc6, 0x3e, 0x0,

    /* U+F013 "" */
    0x18, 0xe, 0x1f, 0xde, 0xf6, 0x33, 0x3b, 0xfc,
    0xba, 0x1c, 0x0,

    /* U+F015 "" */
    0xd, 0x7, 0xc3, 0xd1, 0x7a, 0xbf, 0x4f, 0xc3,
    0x30, 0xcc,

    /* U+F019 "" */
    0x18, 0xc, 0x6, 0xb, 0xc3, 0xc1, 0xe3, 0xfd,
    0xfd, 0xff, 0x80,

    /* U+F01C "" */
    0x3f, 0x18, 0x64, 0xb, 0x3, 0xf3, 0xff, 0xff,
    0xfc,

    /* U+F021 "" */
    0x1e, 0xb1, 0xd1, 0xf8, 0xf0, 0x7, 0x8b, 0x5,
    0xc4, 0xbc, 0x0,

    /* U+F026 "" */
    0x8, 0xff, 0xff, 0x8c, 0x20,

    /* U+F027 "" */
    0x8, 0x33, 0xef, 0xdf, 0xa3, 0x2, 0x0,

    /* U+F028 "" */
    0x1, 0x1, 0x18, 0x65, 0x7d, 0x5f, 0xab, 0xf5,
    0x46, 0x50, 0x42, 0x0, 0x80,

    /* U+F03E "" */
    0xff, 0xcf, 0xe7, 0xff, 0x3d, 0xc, 0x7, 0xfe,

    /* U+F048 "" */
    0x8c, 0xef, 0xff, 0xde, 0x71,

    /* U+F04B "" */
    0xc0, 0xe0, 0xf8, 0xfe, 0xff, 0xfe, 0xfc, 0xf0,
    0xc0,

    /* U+F04C "" */
    0xef, 0xdf, 0xbf, 0x7e, 0xfd, 0xfb, 0xf7,

    /* U+F04D "" */
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

    /* U+F051 "" */
    0x8e, 0x7b, 0xff, 0xf7, 0x31,

    /* U+F052 "" */
    0x0, 0x18, 0x3c, 0x7e, 0xff, 0x0, 0xff, 0xff,

    /* U+F053 "" */
    0x9, 0x99, 0x86, 0x8, 0x20,

    /* U+F054 "" */
    0x43, 0xc, 0x33, 0x31, 0x0,

    /* U+F067 "" */
    0x18, 0x18, 0x18, 0xff, 0xff, 0x18, 0x18, 0x18,

    /* U+F068 "" */
    0xff,

    /* U+F06E "" */
    0x1f, 0xe, 0x39, 0x9b, 0x77, 0x76, 0xec, 0xe3,
    0x87, 0xc0,

    /* U+F070 "" */
    0xc0, 0x6, 0xf0, 0x19, 0xc0, 0xfe, 0x67, 0xe7,
    0x3c, 0x38, 0x80, 0xe6, 0x0, 0x30,

    /* U+F071 "" */
    0xc, 0x3, 0x1, 0xe0, 0x4c, 0x33, 0x1c, 0xe7,
    0xfb, 0xcf, 0xff, 0xc0,

    /* U+F074 "" */
    0x1, 0x73, 0xcb, 0x43, 0x3, 0x57, 0x3c, 0x4,

    /* U+F077 "" */
    0x10, 0x18, 0x3c, 0x66, 0xc3,

    /* U+F078 "" */
    0x81, 0xc3, 0x66, 0x3c, 0x18,

    /* U+F079 "" */
    0x20, 0x7, 0x7c, 0xf8, 0x42, 0x4, 0x21, 0xf3,
    0xee, 0x0, 0x40,

    /* U+F07B "" */
    0xf0, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xfc,

    /* U+F093 "" */
    0x8, 0x7, 0x3, 0xe0, 0xf0, 0xc, 0x3, 0xe,
    0xfb, 0xf5, 0xff, 0xc0,

    /* U+F095 "" */
    0x3, 0x1, 0xc1, 0xe0, 0x70, 0x30, 0x3b, 0xb9,
    0xf8, 0xf0, 0x0,

    /* U+F0C4 "" */
    0xe2, 0xae, 0xfc, 0x38, 0xfc, 0xae, 0xe2,

    /* U+F0C5 "" */
    0x3e, 0x3c, 0xff, 0xff, 0xff, 0xff, 0xff, 0xc0,
    0xfc,

    /* U+F0C7 "" */
    0xfe, 0x83, 0x83, 0xff, 0xe7, 0xe7, 0xff,

    /* U+F0E7 "" */
    0x73, 0xce, 0x3f, 0xf8, 0xc3, 0x8, 0x20,

    /* U+F0EA "" */
    0x20, 0xd8, 0xf8, 0xd8, 0xdb, 0xdf, 0xdf, 0x1f,
    0x1f,

    /* U+F0F3 "" */
    0x10, 0x3c, 0x7e, 0x7e, 0x7e, 0x7e, 0xff, 0x0,
    0x18,

    /* U+F11C "" */
    0xff, 0xeb, 0x5f, 0xff, 0x5b, 0xff, 0xe8, 0x5f,
    0xfc,

    /* U+F124 "" */
    0x1, 0x83, 0xc7, 0xcf, 0xef, 0xe0, 0x70, 0x38,
    0x18, 0xc, 0x0,

    /* U+F15B "" */
    0xf1, 0xeb, 0xc7, 0xff, 0xff, 0xff, 0xff, 0xfe,

    /* U+F1EB "" */
    0x1f, 0xf, 0xfb, 0x1, 0x8f, 0x83, 0x8, 0xc,
    0x1, 0x80,

    /* U+F240 "" */
    0xff, 0xf0, 0xe, 0xfe, 0xdf, 0xff, 0xfe,

    /* U+F241 "" */
    0xff, 0xf0, 0xe, 0xf8, 0xdf, 0x3f, 0xfe,

    /* U+F242 "" */
    0xff, 0xf0, 0xe, 0xf0, 0xde, 0x3f, 0xfe,

    /* U+F243 "" */
    0xff, 0xf0, 0xe, 0xc0, 0xd8, 0x3f, 0xfe,

    /* U+F244 "" */
    0xff, 0xf0, 0xe, 0x0, 0xc0, 0x3f, 0xfe,

    /* U+F287 "" */
    0x6, 0x1, 0x43, 0x41, 0x7f, 0xfc, 0x84, 0xb,
    0x0, 0xe0,

    /* U+F293 "" */
    0x78, 0xdb, 0x96, 0x3e, 0xf8, 0xf9, 0x36, 0x78,

    /* U+F2ED "" */
    0x3c, 0xff, 0x7e, 0x5a, 0x5a, 0x5a, 0x5a, 0x5a,
    0x7e,

    /* U+F304 "" */
    0x3, 0x1, 0xc3, 0x63, 0xc3, 0xe3, 0xe3, 0xe1,
    0xe0, 0xe0, 0x0,

    /* U+F55A "" */
    0x1f, 0xe7, 0xfd, 0xeb, 0xfe, 0xf7, 0xae, 0x7f,
    0xc7, 0xf8,

    /* U+F7C2 "" */
    0x3e, 0xd7, 0xaf, 0xff, 0xff, 0xff, 0xff, 0xfe,

    /* U+F8A2 "" */
    0x0, 0x0, 0x58, 0x3f, 0xf6, 0x0, 0x0
};


/*---------------------
 *  GLYPH DESCRIPTION
 *--------------------*/

static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 36, .box_w = 1, .box_h = 1, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 1, .adv_w = 37, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 2, .adv_w = 46, .box_w = 2, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 3, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 8, .adv_w = 81, .box_w = 4, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 12, .adv_w = 105, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 18, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 23, .adv_w = 25, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = 5},
    {.bitmap_index = 24, .adv_w = 49, .box_w = 3, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 28, .adv_w = 50, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 32, .adv_w = 62, .box_w = 4, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 34, .adv_w = 82, .box_w = 5, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 38, .adv_w = 28, .box_w = 1, .box_h = 2, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 39, .adv_w = 40, .box_w = 2, .box_h = 1, .ofs_x = 0, .ofs_y = 2},
    {.bitmap_index = 40, .adv_w = 38, .box_w = 1, .box_h = 1, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 41, .adv_w = 59, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 44, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 48, .adv_w = 81, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 51, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 55, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 59, .adv_w = 81, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 64, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 68, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 72, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 76, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 80, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 84, .adv_w = 35, .box_w = 1, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 85, .adv_w = 30, .box_w = 2, .box_h = 6, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 87, .adv_w = 73, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 89, .adv_w = 79, .box_w = 3, .box_h = 3, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 91, .adv_w = 75, .box_w = 4, .box_h = 4, .ofs_x = 1, .ofs_y = 1},
    {.bitmap_index = 93, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 97, .adv_w = 129, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 105, .adv_w = 94, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 111, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 116, .adv_w = 94, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 121, .adv_w = 94, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 126, .adv_w = 82, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 130, .adv_w = 80, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 134, .adv_w = 98, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 139, .adv_w = 103, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 144, .adv_w = 39, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 145, .adv_w = 79, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 149, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 154, .adv_w = 77, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 158, .adv_w = 126, .box_w = 6, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 164, .adv_w = 103, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 169, .adv_w = 99, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 174, .adv_w = 91, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 179, .adv_w = 99, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 184, .adv_w = 89, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 188, .adv_w = 85, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 193, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 198, .adv_w = 93, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 203, .adv_w = 92, .box_w = 6, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 209, .adv_w = 128, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 216, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 221, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 226, .adv_w = 86, .box_w = 5, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 231, .adv_w = 38, .box_w = 2, .box_h = 9, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 234, .adv_w = 59, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 238, .adv_w = 38, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 241, .adv_w = 60, .box_w = 3, .box_h = 4, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 243, .adv_w = 65, .box_w = 4, .box_h = 1, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 244, .adv_w = 45, .box_w = 2, .box_h = 2, .ofs_x = 0, .ofs_y = 6},
    {.bitmap_index = 245, .adv_w = 78, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 248, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 252, .adv_w = 75, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 255, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 259, .adv_w = 76, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 262, .adv_w = 50, .box_w = 3, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 265, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 269, .adv_w = 79, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 273, .adv_w = 35, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 274, .adv_w = 34, .box_w = 2, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 277, .adv_w = 73, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 281, .adv_w = 35, .box_w = 1, .box_h = 7, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 282, .adv_w = 126, .box_w = 7, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 287, .adv_w = 79, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 290, .adv_w = 82, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 293, .adv_w = 81, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 297, .adv_w = 82, .box_w = 4, .box_h = 7, .ofs_x = 1, .ofs_y = -2},
    {.bitmap_index = 301, .adv_w = 49, .box_w = 2, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 303, .adv_w = 74, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 306, .adv_w = 47, .box_w = 3, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 309, .adv_w = 79, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 312, .adv_w = 70, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 315, .adv_w = 108, .box_w = 7, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 320, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 323, .adv_w = 68, .box_w = 4, .box_h = 7, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 327, .adv_w = 71, .box_w = 4, .box_h = 5, .ofs_x = 1, .ofs_y = 0},
    {.bitmap_index = 330, .adv_w = 49, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 334, .adv_w = 35, .box_w = 1, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 335, .adv_w = 49, .box_w = 3, .box_h = 9, .ofs_x = 0, .ofs_y = -2},
    {.bitmap_index = 339, .adv_w = 98, .box_w = 5, .box_h = 2, .ofs_x = 1, .ofs_y = 2},
    {.bitmap_index = 341, .adv_w = 54, .box_w = 2, .box_h = 3, .ofs_x = 1, .ofs_y = 4},
    {.bitmap_index = 342, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 353, .adv_w = 144, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 362, .adv_w = 144, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 370, .adv_w = 144, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 378, .adv_w = 99, .box_w = 6, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 383, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 394, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 405, .adv_w = 162, .box_w = 10, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 415, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 426, .adv_w = 162, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 435, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 446, .adv_w = 72, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 451, .adv_w = 108, .box_w = 7, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 458, .adv_w = 162, .box_w = 11, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 471, .adv_w = 144, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 479, .adv_w = 126, .box_w = 5, .box_h = 8, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 484, .adv_w = 126, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 493, .adv_w = 126, .box_w = 7, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 500, .adv_w = 126, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 508, .adv_w = 126, .box_w = 5, .box_h = 8, .ofs_x = 2, .ofs_y = -1},
    {.bitmap_index = 513, .adv_w = 126, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 521, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 526, .adv_w = 90, .box_w = 5, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 531, .adv_w = 126, .box_w = 8, .box_h = 8, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 539, .adv_w = 126, .box_w = 8, .box_h = 1, .ofs_x = 0, .ofs_y = 3},
    {.bitmap_index = 540, .adv_w = 162, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 550, .adv_w = 180, .box_w = 12, .box_h = 9, .ofs_x = -1, .ofs_y = -1},
    {.bitmap_index = 564, .adv_w = 162, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 576, .adv_w = 144, .box_w = 9, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 584, .adv_w = 126, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 589, .adv_w = 126, .box_w = 8, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 594, .adv_w = 180, .box_w = 12, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 605, .adv_w = 144, .box_w = 9, .box_h = 6, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 612, .adv_w = 144, .box_w = 10, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 624, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 635, .adv_w = 126, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 642, .adv_w = 126, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 651, .adv_w = 126, .box_w = 8, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 658, .adv_w = 90, .box_w = 6, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 665, .adv_w = 126, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 674, .adv_w = 126, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 683, .adv_w = 162, .box_w = 10, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 692, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 703, .adv_w = 108, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 711, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 721, .adv_w = 180, .box_w = 11, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 728, .adv_w = 180, .box_w = 11, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 735, .adv_w = 180, .box_w = 11, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 742, .adv_w = 180, .box_w = 11, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 749, .adv_w = 180, .box_w = 11, .box_h = 5, .ofs_x = 0, .ofs_y = 1},
    {.bitmap_index = 756, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 766, .adv_w = 126, .box_w = 7, .box_h = 9, .ofs_x = 1, .ofs_y = -1},
    {.bitmap_index = 774, .adv_w = 126, .box_w = 8, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 783, .adv_w = 144, .box_w = 9, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 794, .adv_w = 180, .box_w = 11, .box_h = 7, .ofs_x = 0, .ofs_y = 0},
    {.bitmap_index = 804, .adv_w = 108, .box_w = 7, .box_h = 9, .ofs_x = 0, .ofs_y = -1},
    {.bitmap_index = 812, .adv_w = 145, .box_w = 9, .box_h = 6, .ofs_x = 0, .ofs_y = 0}
};

/*---------------------
 *  CHARACTER MAPPING
 *--------------------*/

static const uint16_t unicode_list_1[] = {
    0x0, 0xef51, 0xef58, 0xef5b, 0xef5c, 0xef5d, 0xef61, 0xef63,
    0xef65, 0xef69, 0xef6c, 0xef71, 0xef76, 0xef77, 0xef78, 0xef8e,
    0xef98, 0xef9b, 0xef9c, 0xef9d, 0xefa1, 0xefa2, 0xefa3, 0xefa4,
    0xefb7, 0xefb8, 0xefbe, 0xefc0, 0xefc1, 0xefc4, 0xefc7, 0xefc8,
    0xefc9, 0xefcb, 0xefe3, 0xefe5, 0xf014, 0xf015, 0xf017, 0xf037,
    0xf03a, 0xf043, 0xf06c, 0xf074, 0xf0ab, 0xf13b, 0xf190, 0xf191,
    0xf192, 0xf193, 0xf194, 0xf1d7, 0xf1e3, 0xf23d, 0xf254, 0xf4aa,
    0xf712, 0xf7f2
};

/*Collect the unicode lists and glyph_id offsets*/
static const lv_font_fmt_txt_cmap_t cmaps[] =
{
    {
        .range_start = 32, .range_length = 95, .glyph_id_start = 1,
        .unicode_list = NULL, .glyph_id_ofs_list = NULL, .list_length = 0, .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
    },
    {
        .range_start = 176, .range_length = 63475, .glyph_id_start = 96,
        .unicode_list = unicode_list_1, .glyph_id_ofs_list = NULL, .list_length = 58, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY
    }
};

/*-----------------
 *    KERNING
 *----------------*/


/*Pair left and right glyphs for kerning*/
static const uint8_t kern_pair_glyph_ids[] =
{
    1, 53,
    3, 3,
    3, 8,
    3, 34,
    3, 66,
    3, 68,
    3, 69,
    3, 70,
    3, 72,
    3, 78,
    3, 79,
    3, 80,
    3, 81,
    3, 82,
    3, 84,
    3, 88,
    8, 3,
    8, 8,
    8, 34,
    8, 66,
    8, 68,
    8, 69,
    8, 70,
    8, 72,
    8, 78,
    8, 79,
    8, 80,
    8, 81,
    8, 82,
    8, 84,
    8, 88,
    9, 55,
    9, 56,
    9, 58,
    13, 3,
    13, 8,
    15, 3,
    15, 8,
    16, 16,
    34, 3,
    34, 8,
    34, 32,
    34, 36,
    34, 40,
    34, 48,
    34, 50,
    34, 53,
    34, 54,
    34, 55,
    34, 56,
    34, 58,
    34, 80,
    34, 85,
    34, 86,
    34, 87,
    34, 88,
    34, 90,
    34, 91,
    35, 53,
    35, 55,
    35, 58,
    36, 10,
    36, 53,
    36, 62,
    36, 94,
    37, 13,
    37, 15,
    37, 34,
    37, 53,
    37, 55,
    37, 57,
    37, 58,
    37, 59,
    38, 53,
    38, 68,
    38, 69,
    38, 70,
    38, 71,
    38, 72,
    38, 80,
    38, 82,
    38, 86,
    38, 87,
    38, 88,
    38, 90,
    39, 13,
    39, 15,
    39, 34,
    39, 43,
    39, 53,
    39, 66,
    39, 68,
    39, 69,
    39, 70,
    39, 72,
    39, 80,
    39, 82,
    39, 83,
    39, 86,
    39, 87,
    39, 90,
    41, 34,
    41, 53,
    41, 57,
    41, 58,
    42, 34,
    42, 53,
    42, 57,
    42, 58,
    43, 34,
    44, 14,
    44, 36,
    44, 40,
    44, 48,
    44, 50,
    44, 68,
    44, 69,
    44, 70,
    44, 72,
    44, 78,
    44, 79,
    44, 80,
    44, 81,
    44, 82,
    44, 86,
    44, 87,
    44, 88,
    44, 90,
    45, 3,
    45, 8,
    45, 34,
    45, 36,
    45, 40,
    45, 48,
    45, 50,
    45, 53,
    45, 54,
    45, 55,
    45, 56,
    45, 58,
    45, 86,
    45, 87,
    45, 88,
    45, 90,
    46, 34,
    46, 53,
    46, 57,
    46, 58,
    47, 34,
    47, 53,
    47, 57,
    47, 58,
    48, 13,
    48, 15,
    48, 34,
    48, 53,
    48, 55,
    48, 57,
    48, 58,
    48, 59,
    49, 13,
    49, 15,
    49, 34,
    49, 43,
    49, 57,
    49, 59,
    49, 66,
    49, 68,
    49, 69,
    49, 70,
    49, 72,
    49, 80,
    49, 82,
    49, 85,
    49, 87,
    49, 90,
    50, 53,
    50, 55,
    50, 56,
    50, 58,
    51, 53,
    51, 55,
    51, 58,
    53, 1,
    53, 13,
    53, 14,
    53, 15,
    53, 34,
    53, 36,
    53, 40,
    53, 43,
    53, 48,
    53, 50,
    53, 52,
    53, 53,
    53, 55,
    53, 56,
    53, 58,
    53, 66,
    53, 68,
    53, 69,
    53, 70,
    53, 72,
    53, 78,
    53, 79,
    53, 80,
    53, 81,
    53, 82,
    53, 83,
    53, 84,
    53, 86,
    53, 87,
    53, 88,
    53, 89,
    53, 90,
    53, 91,
    54, 34,
    55, 10,
    55, 13,
    55, 14,
    55, 15,
    55, 34,
    55, 36,
    55, 40,
    55, 48,
    55, 50,
    55, 62,
    55, 66,
    55, 68,
    55, 69,
    55, 70,
    55, 72,
    55, 80,
    55, 82,
    55, 83,
    55, 86,
    55, 87,
    55, 90,
    55, 94,
    56, 10,
    56, 13,
    56, 14,
    56, 15,
    56, 34,
    56, 53,
    56, 62,
    56, 66,
    56, 68,
    56, 69,
    56, 70,
    56, 72,
    56, 80,
    56, 82,
    56, 83,
    56, 86,
    56, 94,
    57, 14,
    57, 36,
    57, 40,
    57, 48,
    57, 50,
    57, 55,
    57, 68,
    57, 69,
    57, 70,
    57, 72,
    57, 80,
    57, 82,
    57, 86,
    57, 87,
    57, 90,
    58, 7,
    58, 10,
    58, 11,
    58, 13,
    58, 14,
    58, 15,
    58, 34,
    58, 36,
    58, 40,
    58, 43,
    58, 48,
    58, 50,
    58, 52,
    58, 53,
    58, 54,
    58, 55,
    58, 56,
    58, 57,
    58, 58,
    58, 62,
    58, 66,
    58, 68,
    58, 69,
    58, 70,
    58, 71,
    58, 72,
    58, 78,
    58, 79,
    58, 80,
    58, 81,
    58, 82,
    58, 83,
    58, 84,
    58, 85,
    58, 86,
    58, 87,
    58, 89,
    58, 90,
    58, 91,
    58, 94,
    59, 34,
    59, 36,
    59, 40,
    59, 48,
    59, 50,
    59, 68,
    59, 69,
    59, 70,
    59, 72,
    59, 80,
    59, 82,
    59, 86,
    59, 87,
    59, 88,
    59, 90,
    60, 43,
    60, 54,
    66, 3,
    66, 8,
    66, 87,
    66, 90,
    67, 3,
    67, 8,
    67, 87,
    67, 89,
    67, 90,
    67, 91,
    68, 3,
    68, 8,
    70, 3,
    70, 8,
    70, 87,
    70, 90,
    71, 3,
    71, 8,
    71, 10,
    71, 62,
    71, 68,
    71, 69,
    71, 70,
    71, 72,
    71, 82,
    71, 94,
    73, 3,
    73, 8,
    76, 68,
    76, 69,
    76, 70,
    76, 72,
    76, 82,
    78, 3,
    78, 8,
    79, 3,
    79, 8,
    80, 3,
    80, 8,
    80, 87,
    80, 89,
    80, 90,
    80, 91,
    81, 3,
    81, 8,
    81, 87,
    81, 89,
    81, 90,
    81, 91,
    83, 3,
    83, 8,
    83, 13,
    83, 15,
    83, 66,
    83, 68,
    83, 69,
    83, 70,
    83, 71,
    83, 72,
    83, 80,
    83, 82,
    83, 85,
    83, 87,
    83, 88,
    83, 90,
    85, 80,
    87, 3,
    87, 8,
    87, 13,
    87, 15,
    87, 66,
    87, 68,
    87, 69,
    87, 70,
    87, 71,
    87, 72,
    87, 80,
    87, 82,
    88, 13,
    88, 15,
    89, 68,
    89, 69,
    89, 70,
    89, 72,
    89, 80,
    89, 82,
    90, 3,
    90, 8,
    90, 13,
    90, 15,
    90, 66,
    90, 68,
    90, 69,
    90, 70,
    90, 71,
    90, 72,
    90, 80,
    90, 82,
    91, 68,
    91, 69,
    91, 70,
    91, 72,
    91, 80,
    91, 82,
    92, 43,
    92, 54
};

/* Kerning between the respective left and right glyphs
 * 4.4 format which needs to scaled with `kern_scale`*/
static const int8_t kern_pair_values[] =
{
    -3, -8, -8, -8, -4, -4, -4, -4,
    -4, -1, -1, -4, -1, -4, -6, 1,
    -8, -8, -8, -4, -4, -4, -4, -4,
    -1, -1, -4, -1, -4, -6, 1, 1,
    1, 2, -12, -12, -12, -12, -16, -8,
    -8, -4, -1, -1, -1, -1, -9, -1,
    -6, -5, -7, -1, -1, -1, -4, -2,
    -4, 1, -2, -2, -4, -2, -2, -1,
    -1, -7, -7, -1, -2, -2, -2, -3,
    -2, 1, -1, -1, -1, -1, -1, -1,
    -1, -1, -2, -2, -2, -16, -16, -12,
    -19, 1, -2, -1, -1, -1, -1, -1,
    -1, -2, -2, -2, -2, 1, -2, 1,
    -2, 1, -2, 1, -2, -2, -4, -2,
    -2, -2, -2, -2, -2, -2, -2, -2,
    -2, -2, -2, -2, -2, -3, -4, -3,
    -24, -24, 1, -5, -5, -5, -5, -19,
    -4, -12, -10, -17, -3, -9, -6, -9,
    1, -2, 1, -2, 1, -2, 1, -2,
    -7, -7, -1, -2, -2, -2, -3, -2,
    -23, -23, -10, -14, -2, -2, -1, -1,
    -1, -1, -1, -1, -1, 1, 1, 1,
    -3, -2, -1, -2, -6, -1, -3, -3,
    -15, -16, -15, -6, -2, -2, -17, -2,
    -2, -1, 1, 1, 1, 1, -8, -7,
    -7, -7, -7, -8, -8, -7, -8, -7,
    -5, -8, -7, -5, -4, -5, -5, -4,
    -2, 1, -16, -3, -16, -5, -1, -1,
    -1, -1, 1, -3, -3, -3, -3, -3,
    -3, -3, -2, -2, -1, -1, 1, 1,
    -9, -4, -9, -3, 1, 1, -2, -2,
    -2, -2, -2, -2, -2, -1, -1, 1,
    -3, -2, -2, -2, -2, 1, -2, -2,
    -2, -2, -1, -2, -1, -2, -2, -2,
    1, -3, -15, -4, -15, -7, -2, -2,
    -7, -2, -2, -1, 1, -7, 1, 1,
    1, 1, 1, -5, -5, -5, -5, -2,
    -5, -3, -3, -5, -3, -5, -3, -4,
    -2, -3, -1, -2, -1, -2, 1, 1,
    -2, -2, -2, -2, -1, -1, -1, -1,
    -1, -1, -1, -2, -2, -2, -1, -1,
    -5, -5, -1, -1, -2, -2, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    1, 1, 1, 1, -2, -2, -2, -2,
    -2, 1, -7, -7, -1, -1, -1, -1,
    -1, -7, -7, -7, -7, -10, -10, -1,
    -1, -1, -1, -2, -2, -1, -1, -1,
    -1, 1, 1, -9, -9, -3, -1, -1,
    -1, 1, -1, -1, -1, 4, 1, 1,
    1, -1, 1, 1, -8, -8, -1, -1,
    -1, -1, 1, -1, -1, -1, -9, -9,
    -1, -1, -1, -1, -1, -1, 1, 1,
    -8, -8, -1, -1, -1, -1, 1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1
};

/*Collect the kern pair's data in one place*/
static const lv_font_fmt_txt_kern_pair_t kern_pairs =
{
    .glyph_ids = kern_pair_glyph_ids,
    .values = kern_pair_values,
    .pair_cnt = 434,
    .glyph_ids_size = 0
};

/*--------------------
 *  ALL CUSTOM DATA
 *--------------------*/

/*Store all the custom data of the font*/
static lv_font_fmt_txt_dsc_t font_dsc = {
    .glyph_bitmap = gylph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = cmaps,
    .kern_dsc = &kern_pairs,
    .kern_scale = 16,
    .cmap_num = 2,
    .bpp = 1,
    .kern_classes = 0,
    .bitmap_format = 0
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

/*Initialize a public general font descriptor*/
lv_font_t oled_9_font_symbol = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    /*Function pointer to get glyph's data*/
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    /*Function pointer to get glyph's bitmap*/
    .line_height = 10,          /*The maximum line height required by the font*/
    .base_line = 2,             /*Baseline measured from the bottom of the line*/
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
    .dsc = &font_dsc           /*The custom font data. Will be accessed by `get_glyph_bitmap/dsc` */
};

#endif /*#if OLED_9_FONT_SYMBOL*/

