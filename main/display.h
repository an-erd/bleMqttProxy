#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lvgl/lvgl.h"

#define LV_TICK_PERIOD_MS 1

extern  esp_timer_handle_t display_message_timer;
void oneshot_display_message_timer_callback(void* arg);
void oneshot_display_message_timer_start();
void oneshot_display_message_timer_stop();
void oneshot_display_message_timer_touch();
void display_message_show();
void display_message_stop_show();
void display_create_timer();

#define BEAC_PER_PAGE_LASTSEEN  5

typedef enum {
    SPLASH_SCREEN       = 0,
    BEACON_SCREEN,
    LASTSEEN_SCREEN,
    APPVERSION_SCREEN,
    STATS_SCREEN,
    MAX_SCREEN_NUM,
    UNKNOWN_SCREEN      = 99
} display_screen_t;

// status screen values
typedef struct {
    display_screen_t    current_screen;
    display_screen_t    screen_to_show;
    // Beacon
    uint8_t             current_beac;
    uint8_t             beac_to_show;           // 0..CONFIG_BLE_DEVICE_COUNT_USE-1
    // last seen
    uint8_t             lastseen_page_to_show;  // 1..num_last_seen_pages
    uint8_t             num_last_seen_pages;
    // local Temperature sensor
    uint8_t             localtemp_to_show;      // 1..num_localtemp_pages
    uint8_t             num_localtemp_pages;
    // enable/disable button
    bool                button_enabled;         // will be enabled after splash screen
    // display on/off
    bool                display_on;
    // show message
    bool                display_message;        // show a message on the display for configurable time
    bool                display_message_is_shown;   // currently showing the message
} display_status_t;

extern display_status_t display_status;
extern volatile bool turn_display_off;           // switch display on/off as idle timer action, will be handled in display_update

// pop-up message screen values
typedef struct {
    char                title[32];
    char                message[32];
    char                comment[32];
    char                action[32];
    uint8_t             beac;
    bool                need_refresh;
} display_message_content_t;

extern display_message_content_t display_message_content;

void set_next_display_show();
void display_task(void* pvParameters);
esp_err_t display_update();
void update_display_task(lv_task_t * task);

typedef struct {
    // lv_obj_t * btn1;
    lv_obj_t * label1;

    // lv_obj_t * btn2;
    lv_obj_t * label2;

    // lv_obj_t * btn3;
    lv_obj_t * label3;
} src_buttons_t;

typedef struct {
    lv_obj_t * symbol_battery;
    lv_obj_t * symbol_eye;
} src_symbol_t;

typedef struct {
    lv_obj_t * pagenum;
} src_pagenum_t;

typedef struct {
    lv_obj_t * scr;                     // obj
} src_splash_t;

typedef struct {
    lv_obj_t * scr;                     // obj
    lv_obj_t * name;                    // label
    lv_obj_t * temp_hum;                // label
    lv_obj_t * rssi;                    // label
    lv_obj_t * battery;                 // label
    src_buttons_t buttons;
    src_symbol_t symbols;
    src_pagenum_t pagenum;
} src_beacon_details_t;

typedef struct {
    lv_obj_t * scr;                     // obj
    lv_obj_t * title;                   // label
    lv_obj_t * table;                   // table
    src_buttons_t buttons;
    src_symbol_t symbols;
    src_pagenum_t pagenum;
} src_last_seen_t;

typedef struct {
    lv_obj_t * scr;                     // obj
    lv_obj_t * title;                   // label
    lv_obj_t * table;                   // table
    src_buttons_t buttons;
    src_symbol_t symbols;
    src_pagenum_t pagenum;
} src_app_version_t;

typedef struct {
    lv_obj_t * scr;                     // obj
    lv_obj_t * title;                   // label
    lv_obj_t * table;                   // table
    src_buttons_t buttons;
    src_symbol_t symbols;
    src_pagenum_t pagenum;
} src_stats_t;

typedef struct {
    src_splash_t splash;
    src_beacon_details_t beacon_details;
    src_last_seen_t last_seen;
    src_app_version_t app_version;
    src_stats_t stats;
} lv_screens_t;

extern lv_screens_t lv_screens;

void lv_init_screens();

#endif // __DISPLAY_H__