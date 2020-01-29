#ifndef __TIMER_H__
#define __TIMER_H__

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "esp_timer.h"

// Timer

// The Oneshot timer is used for displaying the splash screen or the idle timer (to switch to empty screen after inactivity)
// The periodic timer is used to update the display regulary, e.g. for displaying last seen/send screen

extern esp_timer_handle_t periodic_timer;
extern esp_timer_handle_t oneshot_timer;

#define UPDATE_LAST_SEEN_INTERVAL       250000      // 4 Hz
#define SPLASH_SCREEN_TIMER_DURATION    2500000     // = 2.5 sec
#define IDLE_TIMER_DURATION             (CONFIG_DISPLAY_IDLE_TIMER * 1000000)

typedef enum {
    TIMER_NO_USAGE = 0,
    TIMER_SPLASH_SCREEN,
    TIMER_IDLE_TIMER
} oneshot_timer_usage_t;

extern oneshot_timer_usage_t oneshot_timer_usage;

void set_run_periodic_timer(bool stat);
bool get_run_periodic_timer();
void periodic_timer_start();
void periodic_timer_stop();
bool periodic_timer_is_running();
void periodic_timer_callback(void* arg);

void set_run_idle_timer(bool stat);
bool get_run_idle_timer();
void idle_timer_start();
void idle_timer_stop();
void set_run_idle_timer_touch(bool stat);
bool get_run_idle_timer_touch();
void idle_timer_touch();
bool idle_timer_is_running();
void oneshot_timer_callback(void* arg);


#endif // __TIMER_H__
