#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#ifdef CONFIG_DISPLAY_SSD1306
#include "ssd1306.h"
#endif

#define UPDATE_DISPLAY      (BIT0)
extern EventGroupHandle_t s_values_evg;

extern  esp_timer_handle_t display_message_timer;
void oneshot_display_message_timer_callback(void* arg);
void oneshot_display_message_timer_start();
void oneshot_display_message_timer_stop();
void oneshot_display_message_timer_touch();
void display_message_show();
void display_message_stop_show();

#define BEAC_PER_PAGE_LASTSEEN  5

typedef enum {
    SPLASH_SCREEN       = 0,
    BEACON_SCREEN,
    LASTSEEN_SCREEN,
    LOCALTEMP_SCREEN,
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
extern volatile bool turn_display_off;           // switch display on/off as idle timer action, will be handled in ssd1306_update

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

#ifdef CONFIG_DISPLAY_SSD1306
extern ssd1306_canvas_t *display_canvas;
extern ssd1306_canvas_t *display_canvas_message;
void ssd1306_task(void* pvParameters);
#endif // CONFIG_DISPLAY_SSD1306

void set_next_display_show();

#endif // __DISPLAY_H__