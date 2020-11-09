#include "blemqttproxy.h"

#include "buttons.h"
#include "helperfunctions.h"
#include "iot_button.h"
#include "beacon.h"
#include "display.h"
#include "stats.h"
#include "timer.h"

static const char* TAG = "BUTTONS";

int8_t btn[MAX_BUTTON_COUNT][2] = { {0, CONFIG_BUTTON_1_PIN}, {1, CONFIG_BUTTON_2_PIN}, {2, CONFIG_BUTTON_3_PIN} };
button_handle_t btn_handle[MAX_BUTTON_COUNT];
int64_t time_button_long_press[MAX_BUTTON_COUNT] = { 0 };

void button_push_cb(void* arg)
{
    uint8_t btn = *((uint8_t*) arg);
    UNUSED(btn);

    if(!display_status.button_enabled){
        ESP_LOGD(TAG, "button_push_cb: button not enabled");
        return;
    }

    time_button_long_press[btn] = esp_timer_get_time() + CONFIG_LONG_PRESS_TIME * 1000;

    ESP_LOGD(TAG, "button_push_cb");
}

void handle_long_button_push()
{
    switch(display_status.current_screen){
        case BEACON_SCREEN:
            toggle_beacon_idx_active(display_status.beac_to_show);
            break;
        case LASTSEEN_SCREEN:
            break;
        case APPVERSION_SCREEN:
            break;
        case STATS_SCREEN:
            clear_stats_values();
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }
}

bool is_button_long_press(uint8_t btn)
{
    return esp_timer_get_time() >= time_button_long_press[btn];
}

void handle_m5stack_button(uint8_t btn, bool long_press)
{
    if((btn == 2) && !long_press){   // right button to switch to next screen
        set_next_display_show();
        return;
    }

    switch(display_status.current_screen){
        case BEACON_SCREEN:
            if(btn==0){
                toggle_beacon_idx_active(display_status.beac_to_show);
            } else if(btn==1){
                clear_beacon_idx_values(display_status.beac_to_show);
            }
            break;
        case LASTSEEN_SCREEN:
            break;
        case APPVERSION_SCREEN:
            break;
        case STATS_SCREEN:
            if(btn==1){
                clear_stats_values();
            }
            break;
        default:
            ESP_LOGE(TAG, "handle_long_button_push: unhandled switch-case");
            break;
    }
}

void button_release_cb(void* arg)
{
    uint8_t btn = *((uint8_t*) arg);
    bool is_long_press;

    ESP_LOGD(TAG, "Button pressed: %d", btn);

    if(!display_status.button_enabled){
        ESP_LOGI(TAG, "button_release_cb: button not enabled");
        return;
    }

    if(!display_status.display_on){
        ESP_LOGI(TAG, "button_release_cb: turn display on again");
        turn_display_on();
        return;
    }

    if(display_status.display_message_is_shown){
        handle_button_display_message();
        return;
    }
    set_run_idle_timer_touch(true);

    ESP_LOGD(TAG, "button_release_cb: display_status.current_screen %d screen_to_show %d >",
        display_status.current_screen, display_status.screen_to_show);

    is_long_press = is_button_long_press(btn);

#if defined CONFIG_DEVICE_WEMOS || defined CONFIG_DEVICE_M5STICK
    if(!is_long_press){
        set_next_display_show();
    } else {
        handle_long_button_push(btn);
    }
    handle_set_next_display_show();
#elif defined CONFIG_DEVICE_M5STICKC
    switch(btn){
        case 0:
            if(!is_long_press){
                set_next_display_show();
            } else {
                handle_long_button_push(btn);
            }
            break;
        case 1:
            handle_long_button_push(btn);   // TODO
            break;
        default:
            ESP_LOGE(TAG, "button_release_cb: unhandled switch case");
    }
    handle_set_next_display_show();
#elif defined CONFIG_DEVICE_M5STACK
    handle_m5stack_button(btn, is_long_press);
#endif

    ESP_LOGD(TAG, "button_release_cb: display_status.current_screen %d screen_to_show %d <",
        display_status.current_screen, display_status.screen_to_show);
}


void initialize_buttons()
{
    for (int i = 0; i < CONFIG_BUTTON_COUNT; i++){
        btn_handle[i] = iot_button_create(btn[i][1], BUTTON_ACTIVE_LEVEL);
        iot_button_set_evt_cb(btn_handle[i], BUTTON_CB_PUSH, button_push_cb, &btn[i][0]);
        iot_button_set_evt_cb(btn_handle[i], BUTTON_CB_RELEASE, button_release_cb, &btn[i][0]);
    }
}
