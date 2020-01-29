#ifndef __DISPLAY_H__
#define __DISPLAY_H__

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define UPDATE_DISPLAY      (BIT0)
extern EventGroupHandle_t s_values_evg;

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
} display_status_t;

extern display_status_t s_display_status;
extern volatile bool turn_display_off;           // switch display on/off as idle timer action, will be handled in ssd1306_update

void ssd1306_task(void* pvParameters);
void set_next_display_show();

#endif // __DISPLAY_H__