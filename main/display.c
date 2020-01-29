#include "esp_log.h"
#include "esp_ota_ops.h"

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
extern EventGroupHandle_t s_wifi_evg;
extern uint16_t wifi_connections_connect;
extern uint16_t wifi_connections_disconnect;
extern const int CONNECTED_BIT;

display_status_t s_display_status = {
    .current_screen = UNKNOWN_SCREEN,
    .screen_to_show = SPLASH_SCREEN,
    .button_enabled = false,
    .display_on = true};

volatile bool turn_display_off = false;

void draw_pagenumber(ssd1306_canvas_t *canvas, uint8_t nr_act, uint8_t nr_total)
{
    char buffer2[5];
    snprintf(buffer2, 5, "%d/%d", nr_act, nr_total);
    ssd1306_draw_string_8x8(canvas, 128 - 3 * 8, 7, (const uint8_t *)buffer2);
}

void set_next_display_show()
{
    ESP_LOGD(TAG, "set_next_display_show: s_display_status.current_screen %d", s_display_status.current_screen);

    switch (s_display_status.current_screen)
    {
    case SPLASH_SCREEN:
        s_display_status.button_enabled = true;
        s_display_status.screen_to_show = BEACON_SCREEN;
        s_display_status.current_beac = UNKNOWN_BEACON;
        s_display_status.beac_to_show = 0;
        set_run_idle_timer(true);
        // idle_timer_start();
        break;

    case BEACON_SCREEN:
        if (s_display_status.beac_to_show < CONFIG_BLE_DEVICE_COUNT_USE - 1)
        {
            s_display_status.beac_to_show++;
        }
        else
        {
            s_display_status.current_beac = UNKNOWN_BEACON;
            s_display_status.screen_to_show = LASTSEEN_SCREEN;
            s_display_status.lastseen_page_to_show = 1;
        }
        break;

    case LASTSEEN_SCREEN:
        if (s_display_status.lastseen_page_to_show < s_display_status.num_last_seen_pages)
        {
            s_display_status.lastseen_page_to_show++;
        }
        else
        {
#if CONFIG_LOCAL_SENSORS_TEMPERATURE == 1
            if (s_owb_num_devices >= CONFIG_MENU_MIN_LOCAL_SENSOR)
            {
                s_display_status.screen_to_show = LOCALTEMP_SCREEN;
                s_display_status.localtemp_to_show = 1;
            }
            else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
            {
                // skip empty local temperature screen
                s_display_status.screen_to_show = APPVERSION_SCREEN;
            }
        }
        break;

    case LOCALTEMP_SCREEN:
#if CONFIG_LOCAL_SENSORS_TEMPERATURE == 1
        if (s_display_status.localtemp_to_show < s_display_status.num_localtemp_pages)
        {
            s_display_status.localtemp_to_show++;
        }
        else
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
        {
            s_display_status.screen_to_show = APPVERSION_SCREEN;
        }
        break;

    case APPVERSION_SCREEN:
        s_display_status.screen_to_show = STATS_SCREEN;
        break;

    case STATS_SCREEN:
        s_display_status.screen_to_show = BEACON_SCREEN;
        s_display_status.current_beac = UNKNOWN_BEACON;
        s_display_status.beac_to_show = 0;
        break;

    default:
        ESP_LOGE(TAG, "set_next_display_show: unhandled switch-case");
        break;
    }
}

esp_err_t ssd1306_update(ssd1306_canvas_t *canvas)
{
    esp_err_t ret;
    UNUSED(ret);
    char buffer[128], buffer2[32];
    EventBits_t uxReturn;

    // ESP_LOGD(TAG, "ssd1306_update >, run_periodic_timer %d, run_idle_timer_touch %d, periodic_timer_is_running %d, ssd1306_update current_screen %d, screen_to_show %d",
    //     run_periodic_timer, run_idle_timer_touch, periodic_timer_running, s_display_status.current_screen, s_display_status.screen_to_show);

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

    // ESP_LOGD(TAG, "ssd1306_update turn_display_off %d, s_display_status.display_on %d ", turn_display_off, s_display_status.display_on);
    if (turn_display_off)
    {
        if (s_display_status.display_on)
        {
            s_display_status.display_on = false;
            ssd1306_display_off();
            return ESP_OK;
        }
    }
    else
    {
        if (!s_display_status.display_on)
        {
            s_display_status.display_on = true;
            ssd1306_display_on();
            return ESP_OK;
        }
    }

    // ESP_LOGD(TAG, "ssd1306_update current_screen %d, screen_to_show %d", s_display_status.current_screen, s_display_status.screen_to_show);

    switch (s_display_status.screen_to_show)
    {

    case SPLASH_SCREEN:
        ESP_LOGD(TAG, "ssd1306_update SPLASH_SCREEN current_screen %d, screen_to_show %d", s_display_status.current_screen, s_display_status.screen_to_show);

        memcpy((void *)canvas->s_chDisplayBuffer, (void *)blemqttproxy_splash1, canvas->w * canvas->h);
        s_display_status.current_screen = s_display_status.screen_to_show;

        return ssd1306_refresh_gram(canvas);
        break;

    case BEACON_SCREEN:
    {

        int idx = s_display_status.beac_to_show;
        if ((s_display_status.current_screen != s_display_status.screen_to_show) || (s_display_status.current_beac != s_display_status.beac_to_show))
        { // TODO
            ssd1306_clear_canvas(canvas, 0x00);
            snprintf(buffer, 128, "%s", ble_beacon_data[idx].name);
            ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
            if (is_beacon_idx_active(idx) && (ble_adv_data[idx].last_seen != 0))
            {
                snprintf(buffer, 128, "%5.2fC, %5.2f%%H", ble_adv_data[idx].temp, ble_adv_data[idx].humidity);
                ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "Batt %4d mV", ble_adv_data[idx].battery);
                ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)buffer, 10, 1);
                snprintf(buffer, 128, "RSSI  %3d dBm", ble_adv_data[idx].measured_power);
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

            s_display_status.current_screen = s_display_status.screen_to_show;
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
        s_display_status.num_last_seen_pages = num_act_beac / BEAC_PER_PAGE_LASTSEEN + (num_act_beac % BEAC_PER_PAGE_LASTSEEN ? 1 : 0) + (!num_act_beac ? 1 : 0);

        if (s_display_status.lastseen_page_to_show > s_display_status.num_last_seen_pages)
        {
            // due to deannouncment by "touching" the beacon - TODO
            s_display_status.lastseen_page_to_show = s_display_status.num_last_seen_pages;
        }

        ssd1306_clear_canvas(canvas, 0x00);

        snprintf(buffer, 128, "Last seen/send:");
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
        if (!num_act_beac)
        {
            s_display_status.lastseen_page_to_show = 1;
            snprintf(buffer, 128, "No active beacon!");
            ssd1306_draw_string(canvas, 0, 10, (const uint8_t *)buffer, 10, 1);
        }
        else
        {
            int skip = (s_display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
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
                        bool never_seen = (ble_adv_data[i].last_seen == 0);
                        if (never_seen)
                        {
                            snprintf(buffer, 128, "%s: %c", ble_beacon_data[i].name, '/');
                        }
                        else
                        {
                            uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_adv_data[i].last_seen) / 1000000;
                            uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_adv_data[i].mqtt_last_send) / 1000000;
                            uint8_t h, m, s, hq, mq, sq;
                            convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                            convert_s_hhmmss(mqtt_last_send_sec_gone, &hq, &mq, &sq);
                            if (h > 99)
                            {
                                snprintf(buffer, 128, "%s: %s", ble_beacon_data[i].name, "seen >99h");
                            }
                            else
                            {
                                snprintf(buffer, 128, "%s: %02d:%02d:%02d %02d:%02d:%02d", ble_beacon_data[i].name, h, m, s, hq, mq, sq);
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
        draw_pagenumber(canvas, s_display_status.lastseen_page_to_show, s_display_status.num_last_seen_pages);
        s_display_status.current_screen = s_display_status.screen_to_show;
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
        draw_pagenumber(canvas, s_display_status.localtemp_to_show, s_display_status.num_localtemp_pages);

        s_display_status.current_screen = s_display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
#endif // CONFIG_LOCAL_SENSORS_TEMPERATURE
        break;

    case APPVERSION_SCREEN:
    {
        const esp_app_desc_t *app_desc = esp_ota_get_app_description();
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

        uxReturn = xEventGroupWaitBits(s_mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
        bool mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

        uxReturn = xEventGroupWaitBits(s_wifi_evg, CONNECTED_BIT, false, true, 0);
        bool wifi_connected = uxReturn & CONNECTED_BIT;

        snprintf(buffer, 128, "WIFI: %s, MQTT: %s/%s", (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
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

        s_display_status.current_screen = s_display_status.screen_to_show;
        return ssd1306_refresh_gram(canvas);
    }
    break;

    case STATS_SCREEN:
    {
        uint16_t uptime_sec = esp_timer_get_time() / 1000000;
        uint8_t up_h, up_m, up_s;

        ssd1306_clear_canvas(canvas, 0x00);

        snprintf(buffer, 128, "%s", "Statistics:");
        ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);

        convert_s_hhmmss(uptime_sec, &up_h, &up_m, &up_s);
        snprintf(buffer, 128, "%-6s:         %3d:%02d:%02d", "uptime", up_h, up_m, up_s);
        ssd1306_draw_string(canvas, 0, 11, (const uint8_t *)buffer, 10, 1);

        snprintf(buffer, 128, "%-9s: %5d/%5d", "WiFi ok/fail", wifi_connections_connect, wifi_connections_disconnect);
        ssd1306_draw_string(canvas, 0, 22, (const uint8_t *)buffer, 10, 1);

        snprintf(buffer, 128, "%-9s: %5d/%5d", "MQTT ok/fail", mqtt_packets_send, mqtt_packets_fail);
        ssd1306_draw_string(canvas, 0, 33, (const uint8_t *)buffer, 10, 1);

        s_display_status.current_screen = s_display_status.screen_to_show;
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
    ssd1306_canvas_t *canvas = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);

    EventBits_t uxBits;
    UNUSED(uxBits);

    i2c_master_init();
    ssd1306_init();

    while (1)
    {
        uxBits = xEventGroupWaitBits(s_values_evg, UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
        ssd1306_update(canvas);
    }
    vTaskDelete(NULL);
}
// #endif // CONFIG_DISPLAY_SSD1306
