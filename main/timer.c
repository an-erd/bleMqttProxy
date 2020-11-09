#include "blemqttproxy.h"

#include "timer.h"
#include "display.h"
#include "watchdog.h"

static const char* TAG = "TIMER";

esp_timer_handle_t periodic_timer;
esp_timer_handle_t oneshot_timer;

oneshot_timer_usage_t oneshot_timer_usage       = { TIMER_NO_USAGE };

static volatile bool periodic_timer_running     = false;    // status of the timer
static volatile bool run_periodic_timer         = false;    // start/stop periodic timer, set during cb, will be handled in display_update

static volatile bool idle_timer_running         = false;    // status of the timer
static volatile bool run_idle_timer             = false;    // start/stop idle timer, set during cb, will be handled in display_update
static volatile bool run_idle_timer_touch       = false;    // touch the idle timer, will be handled in display_update


void set_run_periodic_timer(bool stat) { run_periodic_timer = stat; }
bool get_run_periodic_timer() {return run_periodic_timer; }

void periodic_timer_start()
{
    periodic_timer_running = true;
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, UPDATE_LAST_SEEN_INTERVAL));
}

void periodic_timer_stop()
{
    periodic_timer_running = false;
    ESP_ERROR_CHECK(esp_timer_stop(periodic_timer));
}

bool periodic_timer_is_running()
{
    ESP_LOGD(TAG, "periodic_timer_is_running(), %d", periodic_timer_running);

    return periodic_timer_running;
}

void periodic_timer_callback(void* arg)
{
    // obsolete? refactor
}

void set_run_idle_timer(bool stat) { run_idle_timer = stat;}
bool get_run_idle_timer() {return run_idle_timer; }

void idle_timer_start()
{
    ESP_LOGD(TAG, "idle_timer_start(), idle_timer_running %d, usage %d", idle_timer_running, oneshot_timer_usage);

    assert(oneshot_timer_usage == TIMER_NO_USAGE);

    if(!IDLE_TIMER_DURATION)
        return;

    ESP_LOGD(TAG, "idle_timer_start()");

    oneshot_timer_usage = TIMER_IDLE_TIMER;
    idle_timer_running = true;

    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, IDLE_TIMER_DURATION));
}

void idle_timer_stop()
{
    if(!IDLE_TIMER_DURATION)
        return;

    ESP_LOGD(TAG, "idle_timer_stop(), idle_timer_running %d, usage %d", idle_timer_running, oneshot_timer_usage);

    if(oneshot_timer_usage == TIMER_NO_USAGE){
        idle_timer_running = false;
        return;
    }

    assert(oneshot_timer_usage == TIMER_IDLE_TIMER);

    oneshot_timer_usage = TIMER_NO_USAGE;
    idle_timer_running = false;

    ESP_ERROR_CHECK(esp_timer_stop(oneshot_timer));
}

void set_run_idle_timer_touch(bool stat) { run_idle_timer_touch = stat; }
bool get_run_idle_timer_touch() { return run_idle_timer_touch; }

void idle_timer_touch()
{
    if(!IDLE_TIMER_DURATION)
        return;

    ESP_LOGD(TAG, "idle_timer_touch(), idle_timer_running %d, usage %d", idle_timer_running, oneshot_timer_usage);

    assert(oneshot_timer_usage == TIMER_IDLE_TIMER);

    ESP_ERROR_CHECK(esp_timer_stop(oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_timer, IDLE_TIMER_DURATION));
}

bool idle_timer_is_running()
{
    ESP_LOGD(TAG, "idle_timer_is_running %d, usage %d", idle_timer_running, oneshot_timer_usage);

    return idle_timer_running;
}

void oneshot_timer_callback(void* arg)
{
    oneshot_timer_usage_t usage = *(oneshot_timer_usage_t *)arg;

    ESP_LOGD(TAG, "oneshot_timer_callback: usage %d, oneshot_timer_usage %d", usage, oneshot_timer_usage);

    oneshot_timer_usage = TIMER_NO_USAGE;

    switch(usage)
    {
        case TIMER_NO_USAGE:
            ESP_LOGE(TAG, "oneshot_timer_callback: TIMER_NO_USAGE, should not happen");
            break;

        case TIMER_SPLASH_SCREEN:
            set_next_display_show();
            break;

        case TIMER_IDLE_TIMER:
            run_idle_timer = false;
            run_periodic_timer = false;
            turn_display_off = true;
            break;

        default:
            ESP_LOGE(TAG, "oneshot_timer_callback: unhandled usage, should not happen");
            break;
    }

    oneshot_timer_usage = TIMER_NO_USAGE;
}


void create_timer()
{
    const esp_timer_create_args_t oneshot_timer_args = {
            .callback = &oneshot_timer_callback,
            .arg      = &oneshot_timer_usage,
            .name     = "oneshot"
    };

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &periodic_timer_callback,
            .name     = "periodic"
    };

    const esp_timer_create_args_t periodic_wdt_timer_args = {
            .callback = &periodic_wdt_timer_callback,
            .name     = "periodic_wdt"
    };

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_timer_args, &oneshot_timer));
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    ESP_ERROR_CHECK(esp_timer_create(&periodic_wdt_timer_args, &periodic_wdt_timer));
}

