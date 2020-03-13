#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"

#include "display.h"
#include "beacon.h"
#include "timer.h"
#include "helperfunctions.h"
#include "ble_mqtt.h"

#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "splashscreen.h"

static const char *TAG = "display";

// #ifdef CONFIG_DISPLAY_SSD1306

EventGroupHandle_t s_values_evg;
extern EventGroupHandle_t wifi_evg;
extern uint16_t wifi_connections_count_connect;
extern uint16_t wifi_connections_count_disconnect;
#define WIFI_CONNECTED_BIT          (BIT0)

esp_timer_handle_t oneshot_display_message_timer;
#define DISPLAY_MESSAGE_TIME_DURATION       (CONFIG_DISPLAY_MESSAGE_TIME * 1000000)
void oneshot_display_message_timer_callback(void* arg);
static bool idle_timer_running_before_message = false;
static bool turn_display_off_before_message = false;

display_status_t display_status = {
    .current_screen = UNKNOWN_SCREEN,
    .screen_to_show = SPLASH_SCREEN,
    .button_enabled = false,
    .display_on = true,
    .display_message = false,
    .display_message_is_shown = false
};

display_message_content_t display_message_content =
{
    .title = "",
    .message = "",
    .comment = "",
    .action = "",
    .beac = UNKNOWN_BEACON,
};

volatile bool turn_display_off = false;

ssd1306_canvas_t *display_canvas;
ssd1306_canvas_t *display_canvas_message;

void oneshot_display_message_timer_callback(void* arg)
{
    ESP_LOGD(TAG, "oneshot_display_message_timer_callback");
    display_message_stop_show();
}

void oneshot_display_message_timer_start()
{
    ESP_ERROR_CHECK(esp_timer_start_once(oneshot_display_message_timer, DISPLAY_MESSAGE_TIME_DURATION));
}

void oneshot_display_message_timer_stop()
{
    esp_err_t ret;

    ret = esp_timer_stop(oneshot_display_message_timer);
    if(ret == ESP_ERR_INVALID_STATE){
        return;
    }

    ESP_ERROR_CHECK(ret);
}

void oneshot_display_message_timer_touch()
{
    oneshot_display_message_timer_stop();
    oneshot_display_message_timer_start();
}

void display_message_show()
{
    turn_display_off_before_message = turn_display_off;
    idle_timer_running_before_message = get_run_idle_timer();

    ESP_LOGD(TAG, "display_message_show, turn_display_off_before_message %d, idle_timer_running_before_message %d",
        turn_display_off_before_message, idle_timer_running_before_message);
    set_run_idle_timer(false);
    turn_display_off = false;
    display_status.display_message = true;
    oneshot_display_message_timer_start();
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void display_message_stop_show()
{
    ESP_LOGD(TAG, "display_message_stop_show, turn_display_off_before_message %d, idle_timer_running_before_message %d",
        turn_display_off_before_message, idle_timer_running_before_message);
    oneshot_display_message_timer_stop();
    turn_display_off = turn_display_off_before_message;
    set_run_idle_timer(idle_timer_running_before_message);
    display_status.display_message = false;
    xEventGroupSetBits(s_values_evg, UPDATE_DISPLAY);
}

void draw_pagenumber(ssd1306_canvas_t *canvas, uint8_t nr_act, uint8_t nr_total)
{
    char buffer2[5];
    snprintf(buffer2, 5, "%d/%d", nr_act, nr_total);
    ssd1306_draw_string_8x8(canvas, 128 - 3 * 8, 7, (const uint8_t *)buffer2);
}

void update_display_message(ssd1306_canvas_t *canvas)
{
    char buffer[128], buffer2[32];

    ESP_LOGD(TAG, "update_display_message");

    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)display_message_content.title, 10, 1);
    ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)display_message_content.message, 10, 1);
    ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)display_message_content.comment, 10, 1);
    ssd1306_draw_string(canvas, 0, 48, (const uint8_t *)display_message_content.action, 10, 1);
}

 void set_next_display_show()
{
    ESP_LOGD(TAG, "set_next_display_show: display_status.current_screen %d", display_status.current_screen);

    switch (display_status.current_screen)
    {
    case SPLASH_SCREEN:
        display_status.button_enabled = true;
        display_status.screen_to_show = BEACON_SCREEN;
        display_status.current_beac = UNKNOWN_BEACON;
        display_status.beac_to_show = 0;
        set_run_idle_timer(true);
        // idle_timer_start();
        break;

    case BEACON_SCREEN:
        if (display_status.beac_to_show < CONFIG_BLE_DEVICE_COUNT_USE - 1)
        {
            display_status.beac_to_show++;
        }
        else
        {
            display_status.current_beac = UNKNOWN_BEACON;
            display_status.screen_to_show = LASTSEEN_SCREEN;
            display_status.lastseen_page_to_show = 1;
        }
        break;

    case LASTSEEN_SCREEN:
        if (display_status.lastseen_page_to_show < display_status.num_last_seen_pages)
        {
            display_status.lastseen_page_to_show++;
        }
        else
        {
#if CONFIG_LOCAL_SENSORS_TEMPERATURE == 1
            if (s_owb_num_devices >= CONFIG_MENU_MIN_LOCAL_SENSOR)
            {
                display_status.screen_to_show = LOCALTEMP_SCREEN;
                display_status.localtemp_to_show = 1;
            }
            else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
            {
                // skip empty local temperature screen
                display_status.screen_to_show = APPVERSION_SCREEN;
            }
        }
        break;

    case LOCALTEMP_SCREEN:
#if CONFIG_LOCAL_SENSORS_TEMPERATURE == 1
        if (display_status.localtemp_to_show < display_status.num_localtemp_pages)
        {
            display_status.localtemp_to_show++;
        }
        else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
        {
            display_status.screen_to_show = APPVERSION_SCREEN;
        }
        break;

    case APPVERSION_SCREEN:
        display_status.screen_to_show = STATS_SCREEN;
        break;

    case STATS_SCREEN:
        display_status.screen_to_show = BEACON_SCREEN;
        display_status.current_beac = UNKNOWN_BEACON;
        display_status.beac_to_show = 0;
        break;

    default:
        ESP_LOGE(TAG, "set_next_display_show: unhandled switch-case");
        break;
    }
}

esp_err_t ssd1306_update(ssd1306_canvas_t *canvas, ssd1306_canvas_t *canvas_message)
{
    esp_err_t ret;
    UNUSED(ret);
    char buffer[128], buffer2[32];
    EventBits_t uxReturn;

    // ESP_LOGD(TAG, "ssd1306_update >, run_periodic_timer %d, run_idle_timer_touch %d, periodic_timer_is_running %d, ssd1306_update current_screen %d, screen_to_show %d",
    //     run_periodic_timer, run_idle_timer_touch, periodic_timer_running, display_status.current_screen, display_status.screen_to_show);

    // ESP_ERROR_CHECK(esp_timer_dump(stdout));

    if (get_run_periodic_timer())
    {
        if (!periodic_timer_is_running())
        {
            periodic_timer_start();
        }
    }
    else
    {
        if (periodic_timer_is_running())
        {
            periodic_timer_stop();
        }
    }

    if (get_run_idle_timer())
    {
        if (!idle_timer_is_running())
        {
            idle_timer_start();
        }
    }
    else
    {
        if (idle_timer_is_running())
        {
            idle_timer_stop();
        }
    }

    if (get_run_idle_timer_touch())
    {
        set_run_idle_timer_touch(false);
        ESP_LOGD(TAG, "ssd1306_update idle_timer_is_running() = %d", idle_timer_is_running());

        if (idle_timer_is_running())
        {
            idle_timer_touch();
        }
    }

    // ESP_LOGD(TAG, "ssd1306_update turn_display_off %d, display_status.display_on %d ", turn_display_off, display_status.display_on);
    if (turn_display_off)
    {
        if (display_status.display_on)
        {
            display_status.display_on = false;
            ssd1306_display_off();
            return ESP_OK;
        }
    }
    else
    {
        if (!display_status.display_on)
        {
            display_status.display_on = true;
            ssd1306_display_on();
        }
    }

    // ESP_LOGD(TAG, "ssd1306_update display_message %d, display_message_is_shown %d", display_status.display_message, display_status.display_message_is_shown);
    if(display_status.display_message){
        if(display_status.display_message_is_shown && !display_message_content.need_refresh){
            // SOLL: anzeigen, IST: wird gezeigt
            return ESP_OK;
        } else {
            // SOLL: anzeigen, IST: wird nicht gezeigt
            display_status.display_message_is_shown = true;
            display_message_content.need_refresh = false;
            update_display_message(canvas_message);
            return ssd1306_refresh_gram(canvas_message);
        }
    } else {
        if(display_status.display_message_is_shown){
            // SOLL: nicht anzeigen, IST: wird gezeigt
            display_status.current_screen = UNKNOWN_SCREEN;
            display_status.display_message_is_shown = false;
        } else {
            // SOLL: nicht anzeigen, IST: nicht gezeigt
        }
    }

    // ESP_LOGD(TAG, "ssd1306_update current_screen %d, screen_to_show %d", display_status.current_screen, display_status.screen_to_show);

    switch (display_status.screen_to_show)
    {

    case SPLASH_SCREEN:
        ESP_LOGD(TAG, "ssd1306_update SPLASH_SCREEN current_screen %d, screen_to_show %d", display_status.current_screen, display_status.screen_to_show);

        memcpy((void *)canvas->s_chDisplayBuffer, (void *)blemqttproxy_splash1, canvas->w * canvas->h);
        display_status.current_screen = display_status.screen_to_show;

        return ssd1306_refresh_gram(canvas);
        break;

    case BEACON_SCREEN:
    {

        int idx = display_status.beac_to_show;
        if ((display_status.current_screen != display_status.screen_to_show)
            || (display_status.current_beac != display_status.beac_to_show))
        {
            ssd1306_clear_canvas(canvas, 0x00);
            snprintf(buffer, 128, "%s", ble_beacons[idx].beacon_data.name);
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
            if (is_beacon_idx_active(idx) && (ble_beacons[idx].adv_data.last_seen != 0))
            {
                snprintf(buffer, 128, "%5.2fC, %5.2f%%H",
                    ble_beacons[idx].adv_data.temp, ble_beacons[idx].adv_data.humidity);
                ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "Batt %4d mV", ble_beacons[idx].adv_data.battery);
                ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "RSSI  %3d dBm", ble_beacons[idx].adv_data.measured_power);
                ssd1306_draw_string(canvas, 0, 36, (const uint8_t *)buffer, 10, 1);
            }
            else
            {
                snprintf(buffer, 128, "  -  C,   -  %%H");
                ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "Batt   -  mV");
                ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "RSSI   -  dBm");
                ssd1306_draw_string(canvas, 0, 36, (const uint8_t *)buffer, 10, 1);
            }
            snprintf(buffer, 128, "active: %s", (is_beacon_idx_active(idx) ? "y" : "n"));
            ssd1306_draw_string(canvas, 0, 48, (const uint8_t *)buffer, 10, 1);

            draw_pagenumber(canvas, idx + 1, CONFIG_BLE_DEVICE_COUNT_USE);

            display_status.current_screen = display_status.screen_to_show;
            return ssd1306_refresh_gram(canvas);
        }
        else
        {
            ESP_LOGD(TAG, "ssd1306_update: not current screen to udate, exit");
            return ESP_OK;
        }
        break;
    }

    case LASTSEEN_SCREEN:
    {
        uint8_t num_act_beac = num_active_beacon();
        display_status.num_last_seen_pages = num_act_beac / BEAC_PER_PAGE_LASTSEEN + (num_act_beac % BEAC_PER_PAGE_LASTSEEN ? 1 : 0) + (!num_act_beac ? 1 : 0);

        if (display_status.lastseen_page_to_show > display_status.num_last_seen_pages)
        {
            // due to deannouncment by "touching" the beacon - TODO
            display_status.lastseen_page_to_show = display_status.num_last_seen_pages;
        }

        ssd1306_clear_canvas(canvas, 0x00);

        snprintf(buffer, 128, "Last seen/send:");
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
        if (!num_act_beac)
        {
            display_status.lastseen_page_to_show = 1;
            snprintf(buffer, 128, "No active beacon!");
            ssd1306_draw_string(canvas, 0, 10, (const uint8_t *)buffer, 10, 1);
        }
        else
        {
            int skip = (display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
            int line = 1;
            for (int i = 0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++)
            {
                if (is_beacon_idx_active(i))
                {
                    if (skip)
                    {
                        skip--;
                    }
                    else
                    {
                        bool never_seen = (ble_beacons[i].adv_data.last_seen == 0);
                        if (never_seen)
                        {
                            snprintf(buffer, 128, "%s: %c", ble_beacons[i].beacon_data.name, '/');
                        }
                        else
                        {
                            uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.last_seen) / 1000000;
                            uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.mqtt_last_send) / 1000000;
                            uint8_t h, m, s, hq, mq, sq;
                            convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                            convert_s_hhmmss(mqtt_last_send_sec_gone, &hq, &mq, &sq);
                            if (h > 99)
                            {
                                snprintf(buffer, 128, "%s: %s", ble_beacons[i].beacon_data.name, "seen >99h");
                            }
                            else
                            {
                                snprintf(buffer, 128, "%s: %02d:%02d:%02d %02d:%02d:%02d", ble_beacons[i].beacon_data.name, h, m, s, hq, mq, sq);
                            }
                        }
                        ssd1306_draw_string(canvas, 0, line * 10, (const uint8_t *)buffer, 10, 1);
                        if (BEAC_PER_PAGE_LASTSEEN == line++)
                        {
                            break;
                        };
                    }
                }
            }
        }
        draw_pagenumber(canvas, display_status.lastseen_page_to_show, display_status.num_last_seen_pages);
        display_status.current_screen = display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
        break;
    }

    case LOCALTEMP_SCREEN:
#if CONFIG_LOCAL_SENSORS_TEMPERATURE == 1
        ssd1306_clear_canvas(canvas, 0x00);
        if (s_owb_num_devices == 0)
        {
            snprintf(buffer, 128, "No local temperature!");
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
        }
        else
        {
            // localtemp_to_show
        }
        draw_pagenumber(canvas, display_status.localtemp_to_show, display_status.num_localtemp_pages);

        display_status.current_screen = display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
        break;

    case APPVERSION_SCREEN:
    {
        const esp_app_desc_t *app_desc = esp_ota_get_app_description();
        tcpip_adapter_ip_info_t ipinfo;
        uint8_t mac[6];
        ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

        ssd1306_clear_canvas(canvas, 0x00);
        snprintf(buffer, 128, "%s", app_desc->version);
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 128, "%s", app_desc->project_name);
        ssd1306_draw_string(canvas, 0, 11, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 128, "%s", app_desc->idf_ver);
        ssd1306_draw_string(canvas, 0, 22, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 128, "%2X:%2X:%2X:%2X:%2X:%2X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        ssd1306_draw_string(canvas, 0, 33, (const uint8_t *)buffer, 10, 1);

        tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipinfo);
        sprintf(buffer, IPSTR, IP2STR(&ipinfo.ip));
        ssd1306_draw_string(canvas, 0, 44, (const uint8_t *)buffer, 10, 1);

        itoa(s_active_beacon_mask, buffer2, 2);
        int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
        if (!num_lead_zeros)
        {
            snprintf(buffer, 128, "Act:  %s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
        }
        else
        {
            snprintf(buffer, 128, "Act:  %0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
        }
        ssd1306_draw_string(canvas, 0, 55, (const uint8_t *)buffer, 10, 1);

        display_status.current_screen = display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
    }
    break;

    case STATS_SCREEN:
    {
        uint32_t uptime_sec = esp_timer_get_time() / 1000000;
        uint16_t up_d;
        uint8_t up_h, up_m, up_s;

        ssd1306_clear_canvas(canvas, 0x00);

        snprintf(buffer, 128, "%s", "Statistics/Status:");
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);

        convert_s_ddhhmmss(uptime_sec, &up_d, &up_h, &up_m, &up_s);
        snprintf(buffer, 128, "%-6s      %3dd %2d:%02d:%02d", "uptime", up_d, up_h, up_m, up_s);
        ssd1306_draw_string(canvas, 0, 11, (const uint8_t *)buffer, 10, 1);

        snprintf(buffer, 128, "%-9s %6d/%5d", "WiFi ok/fail", wifi_connections_count_connect, wifi_connections_count_disconnect);
        ssd1306_draw_string(canvas, 0, 22, (const uint8_t *)buffer, 10, 1);

        snprintf(buffer, 128, "%-9s %6d/%5d", "MQTT ok/fail", mqtt_packets_send, mqtt_packets_fail);
        ssd1306_draw_string(canvas, 0, 33, (const uint8_t *)buffer, 10, 1);

        uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
        bool mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

        uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
        bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

        snprintf(buffer, 128, "WIFI: %s, MQTT: %s/%s", (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
        ssd1306_draw_string(canvas, 0, 44, (const uint8_t *)buffer, 10, 1);

        display_status.current_screen = display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
    }
    break;

    default:
        ESP_LOGE(TAG, "unhandled ssd1306_update screen");
        break;
    }

    ESP_LOGE(TAG, "ssd1306_update: this line should not be reached");
    return ESP_FAIL;
}

void ssd1306_task(void *pvParameters)
{
    const esp_timer_create_args_t oneshot_display_message_timer_args = {
        .callback = &oneshot_display_message_timer_callback,
        .name     = "oneshot_display_message"
    };

    // canvas for a full screen display and a pop-up message
    display_canvas = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);
    display_canvas_message = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);

    EventBits_t uxBits;
    UNUSED(uxBits);

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_display_message_timer_args, &oneshot_display_message_timer));
    i2c_master_init();
    ssd1306_init();

    while (1)
    {
        uxBits = xEventGroupWaitBits(s_values_evg, UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
        ssd1306_update(display_canvas, display_canvas_message);
    }
    vTaskDelete(NULL);
}
// #endif // CONFIG_DISPLAY_SSD1306
