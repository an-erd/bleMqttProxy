#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include <sys/param.h>

#include "display.h"
#include "beacon.h"
#include "timer.h"
#include "helperfunctions.h"
#include "ble_mqtt.h"
#include "web_file_server.h"

#ifdef CONFIG_DISPLAY_SSD1306
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include "splashscreen.h"
#endif

#ifdef CONFIG_DISPLAY_M5STACK
#include "lvgl/lvgl.h"
#include "ili9341.h"
#endif // CONFIG_DISPLAY_M5STACK

static const char *TAG = "display";


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

#ifdef CONFIG_DISPLAY_SSD1306
ssd1306_canvas_t *display_canvas;
ssd1306_canvas_t *display_canvas_message;
#endif // CONFIG_DISPLAY_SSD1306

#ifdef CONFIG_DISPLAY_M5STACK
    static lv_style_t style_screen;
    static lv_style_t style_title;
    static lv_style_t style_bigvalues;
    static lv_style_t style_cell1;
    static lv_style_t style_cell2;

    LV_IMG_DECLARE(splash);
    lv_screens_t lv_screens;

    LV_FONT_DECLARE(m5stack_16);
    // LV_FONT_DECLARE(m5stack_22);
    LV_FONT_DECLARE(m5stack_22_font_symbol);
    // LV_FONT_DECLARE(m5stack_28);
    // LV_FONT_DECLARE(m5stack_36);
    LV_FONT_DECLARE(m5stack_36_font_symbol);
    LV_FONT_DECLARE(m5stack_48_font_symbol);
    // LV_FONT_DECLARE(m5stack_64);

#define X_BUTTON_A	    65          // display button x position (for center of button)
#define X_BUTTON_B	    160
#define X_BUTTON_C	    255

#endif // CONFIG_DISPLAY_M5STACK

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

#ifdef CONFIG_DISPLAY_SSD1306
void draw_pagenumber(ssd1306_canvas_t *canvas, uint8_t nr_act, uint8_t nr_total)
{
    char buffer2[5];
    snprintf(buffer2, 5, "%d/%d", nr_act, nr_total);
    ssd1306_draw_string_8x8(canvas, 128 - 3 * 8, 7, (const uint8_t *)buffer2);
}

void update_display_message(ssd1306_canvas_t *canvas)
{
    ESP_LOGD(TAG, "update_display_message");

    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)display_message_content.title, 10, 1);
    ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)display_message_content.message, 10, 1);
    ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)display_message_content.comment, 10, 1);
    ssd1306_draw_string(canvas, 0, 48, (const uint8_t *)display_message_content.action, 10, 1);
}
#endif // CONFIG_DISPLAY_SSD1306

#ifdef CONFIG_DISPLAY_M5STACK
void update_display_message_m5stack(bool show)
{
    static bool already_created = false;
    static lv_style_t modal_style, box_style;
    static lv_obj_t *mbox_obj, *mbox1;

    ESP_LOGI(TAG, "update_display_message_m5stack, already_created %d, show %d", already_created, show);

    static const char * btns[] ={"Toggle", ""};

    if(show){
        char message[128];
        snprintf(message, 128, "%s\n\n%s\n%s\n\n%s",
            display_message_content.title,
            display_message_content.message,
            display_message_content.comment,
            display_message_content.action);

        if(!already_created){
            lv_style_copy(&modal_style, &lv_style_plain_color);
    		modal_style.body.main_color = modal_style.body.grad_color = LV_COLOR_WHITE;
	    	modal_style.body.opa = LV_OPA_50;

            /* Create a base object for the modal background */
            mbox_obj = lv_obj_create(lv_scr_act(), NULL);
            lv_obj_set_style(mbox_obj, &modal_style);
            lv_obj_set_pos(mbox_obj, 0, 0);
            lv_obj_set_size(mbox_obj, LV_HOR_RES, LV_VER_RES);
            lv_obj_set_opa_scale_enable(mbox_obj, true); /* Enable opacity scaling for the animation */

            lv_style_copy(&box_style, &lv_style_plain);
            box_style.body.main_color = LV_COLOR_WHITE;
            box_style.body.grad_color = LV_COLOR_WHITE;
            box_style.text.color = LV_COLOR_GRAY;

            mbox1 = lv_mbox_create(mbox_obj, NULL);
            // lv_obj_set_style(mbox_obj, &box_style);
            lv_obj_set_width(mbox1, 280);
            lv_mbox_add_btns(mbox1, btns);
            lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_auto_realign(mbox1, true);
            already_created = true;
        }
        lv_mbox_set_text(mbox1, message);
    } else {
        lv_obj_del(mbox_obj);
        already_created = false;
    }
}
#endif

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
        if (display_status.beac_to_show < CONFIG_BLE_DEVICE_COUNT_USE - 1) {
            display_status.beac_to_show++;
        } else {
            display_status.current_beac = UNKNOWN_BEACON;
            display_status.screen_to_show = LASTSEEN_SCREEN;
            display_status.lastseen_page_to_show = 1;
        }
        break;

    case LASTSEEN_SCREEN:
        if (display_status.lastseen_page_to_show < display_status.num_last_seen_pages) {
            display_status.lastseen_page_to_show++;
        } else {
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

void display_update_check_timer()
{
    // ESP_ERROR_CHECK(esp_timer_dump(stdout));

    if (get_run_periodic_timer()) {
        if (!periodic_timer_is_running())
        {
            periodic_timer_start();
        }
    } else {
        if (periodic_timer_is_running())
        {
            periodic_timer_stop();
        }
    }

    if (get_run_idle_timer()) {
        if (!idle_timer_is_running())
        {
            idle_timer_start();
        }
    } else {
        if (idle_timer_is_running()) {
            idle_timer_stop();
        }
    }

    if (get_run_idle_timer_touch()) {
        set_run_idle_timer_touch(false);
        ESP_LOGD(TAG, "display_update_check_timer idle_timer_is_running() = %d", idle_timer_is_running());

        if (idle_timer_is_running()) {
            idle_timer_touch();
        }
    }
}

// return true if display switched off
bool display_update_check_display_off()
{
#ifdef CONFIG_DISPLAY_M5STACK
    // static lv_obj_t * prev_scr;
#endif

    if (turn_display_off){
        if (display_status.display_on){
            display_status.display_on = false;
#ifdef CONFIG_DISPLAY_SSD1306
            ssd1306_display_off();
#elif CONFIG_DISPLAY_M5STACK
            // prev_scr = lv_disp_get_scr_act(NULL);
            // lv_scr_load(lv_screens.empty.scr);
            ili9341_enable_backlight(false);
            ili9341_sleep_in();
#endif
        }
        return true;
    } else {
        if (!display_status.display_on) {
            display_status.display_on = true;
#ifdef CONFIG_DISPLAY_SSD1306
            ssd1306_display_on();
#elif CONFIG_DISPLAY_M5STACK
            ili9341_sleep_out();
            vTaskDelay(5 / portTICK_PERIOD_MS);
            ili9341_enable_backlight(true);
            // lv_scr_load(prev_scr);
#endif
        }
    }

    return false;
}

#ifdef CONFIG_DISPLAY_SSD1306

esp_err_t ssd1306_show_splash_screen(ssd1306_canvas_t *canvas)
{
    memcpy((void *)canvas->s_chDisplayBuffer, (void *)blemqttproxy_splash1, canvas->w * canvas->h);
    return ssd1306_refresh_gram(canvas);
}

esp_err_t ssd1306_show_beacon_screen(ssd1306_canvas_t *canvas, int idx)
{
    char buffer[32], buffer2[32];

    ssd1306_clear_canvas(canvas, 0x00);

    snprintf(buffer, 32, "%s", ble_beacons[idx].beacon_data.name);
    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
    if (is_beacon_idx_active(idx) && (ble_beacons[idx].adv_data.last_seen != 0)) {
        snprintf(buffer, 32, "%5.2fC, %5.2f%%H",
            ble_beacons[idx].adv_data.temp, ble_beacons[idx].adv_data.humidity);
        ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 32, "Batt %4d mV", ble_beacons[idx].adv_data.battery);
        ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 32, "RSSI  %3d dBm", ble_beacons[idx].adv_data.measured_power);
        ssd1306_draw_string(canvas, 0, 36, (const uint8_t *)buffer, 10, 1);
    } else {
        snprintf(buffer, 32, "  -  C,   -  %%H");
        ssd1306_draw_string(canvas, 0, 12, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 32, "Batt   -  mV");
        ssd1306_draw_string(canvas, 0, 24, (const uint8_t *)buffer, 10, 1);
        snprintf(buffer, 32, "RSSI   -  dBm");
        ssd1306_draw_string(canvas, 0, 36, (const uint8_t *)buffer, 10, 1);
    }
    snprintf(buffer, 32, "active: %s", (is_beacon_idx_active(idx) ? "y" : "n"));
    ssd1306_draw_string(canvas, 0, 48, (const uint8_t *)buffer, 10, 1);

    draw_pagenumber(canvas, idx + 1, CONFIG_BLE_DEVICE_COUNT_USE);

    return ssd1306_refresh_gram(canvas);
}

esp_err_t ssd1306_show_last_seen_screen(ssd1306_canvas_t *canvas, uint8_t num_act_beac)
{
    char buffer[32], buffer2[32];

    ssd1306_clear_canvas(canvas, 0x00);

    snprintf(buffer, 32, "Last seen/send:");
    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
    if (!num_act_beac) {
        display_status.lastseen_page_to_show = 1;
        snprintf(buffer, 32, "No active beacon!");
        ssd1306_draw_string(canvas, 0, 10, (const uint8_t *)buffer, 10, 1);
    } else {
        int skip = (display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
        int line = 1;
        for (int i = 0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++) {
            if (is_beacon_idx_active(i)) {
                if (skip) {
                    skip--;
                } else {
                    bool never_seen = (ble_beacons[i].adv_data.last_seen == 0);
                    if (never_seen)                    {
                        snprintf(buffer, 32, "%s: %c", ble_beacons[i].beacon_data.name, '/');
                    } else {
                        uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.last_seen) / 1000000;
                        uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.mqtt_last_send) / 1000000;
                        uint8_t h, m, s, hq, mq, sq;
                        convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                        convert_s_hhmmss(mqtt_last_send_sec_gone, &hq, &mq, &sq);
                        if (h > 99) {
                            snprintf(buffer, 32, "%s: %s", ble_beacons[i].beacon_data.name, "seen >99h");
                        } else {
                            snprintf(buffer, 32, "%s: %02d:%02d:%02d %02d:%02d:%02d", ble_beacons[i].beacon_data.name, h, m, s, hq, mq, sq);
                        }
                    }
                    ssd1306_draw_string(canvas, 0, line * 10, (const uint8_t *)buffer, 10, 1);
                    if (BEAC_PER_PAGE_LASTSEEN == line++) {
                        break;
                    }
                }
            }
        }
    }

    draw_pagenumber(canvas, display_status.lastseen_page_to_show, display_status.num_last_seen_pages);

    return ssd1306_refresh_gram(canvas);
}

esp_err_t ssd1306_show_app_version_screen(ssd1306_canvas_t *canvas)
{
    char buffer[32], buffer2[32];
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    tcpip_adapter_ip_info_t ipinfo;
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));

    ssd1306_clear_canvas(canvas, 0x00);
    snprintf(buffer, 32, "%s", app_desc->version);
    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);
    snprintf(buffer, 32, "%s", app_desc->project_name);
    ssd1306_draw_string(canvas, 0, 11, (const uint8_t *)buffer, 10, 1);
    snprintf(buffer, 32, "%s", app_desc->idf_ver);
    ssd1306_draw_string(canvas, 0, 22, (const uint8_t *)buffer, 10, 1);
    snprintf(buffer, 32, "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    ssd1306_draw_string(canvas, 0, 33, (const uint8_t *)buffer, 10, 1);

    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipinfo);
    sprintf(buffer, IPSTR, IP2STR(&ipinfo.ip));
    ssd1306_draw_string(canvas, 0, 44, (const uint8_t *)buffer, 10, 1);

    itoa(s_active_beacon_mask, buffer2, 2);
    int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
    if (!num_lead_zeros)
    {
        snprintf(buffer, 32, "Act:  %s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    }
    else
    {
        snprintf(buffer, 32, "Act:  %0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    }
    ssd1306_draw_string(canvas, 0, 55, (const uint8_t *)buffer, 10, 1);

    return ssd1306_refresh_gram(canvas);
}

esp_err_t ssd1306_show_stats_screen(ssd1306_canvas_t *canvas)
{
    char buffer[32], buffer2[32];
    uint32_t uptime_sec = esp_timer_get_time() / 1000000;
    uint16_t up_d;
    uint8_t up_h, up_m, up_s;
    EventBits_t uxReturn;

    ssd1306_clear_canvas(canvas, 0x00);

    snprintf(buffer, 32, "%s", "Statistics/Status:");
    ssd1306_draw_string(canvas, 0, 0, (const uint8_t *)buffer, 10, 1);

    convert_s_ddhhmmss(uptime_sec, &up_d, &up_h, &up_m, &up_s);
    snprintf(buffer, 32, "%-6s      %3dd %2d:%02d:%02d", "uptime", up_d, up_h, up_m, up_s);
    ssd1306_draw_string(canvas, 0, 11, (const uint8_t *)buffer, 10, 1);

    snprintf(buffer, 32, "%-9s %6d/%5d", "WiFi ok/fail", wifi_connections_count_connect, wifi_connections_count_disconnect);
    ssd1306_draw_string(canvas, 0, 22, (const uint8_t *)buffer, 10, 1);

    snprintf(buffer, 32, "%-9s %6d/%5d", "MQTT ok/fail", mqtt_packets_send, mqtt_packets_fail);
    ssd1306_draw_string(canvas, 0, 33, (const uint8_t *)buffer, 10, 1);

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    bool mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;

    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    snprintf(buffer, 32, "WIFI: %s, MQTT: %s/%s", (wifi_connected ? "y" : "n"), (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
    ssd1306_draw_string(canvas, 0, 44, (const uint8_t *)buffer, 10, 1);
    return ssd1306_refresh_gram(canvas);
}


void initialize_ssd1306()
{
    // canvas for a full screen display and a pop-up message
    display_canvas = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);
    display_canvas_message = create_ssd1306_canvas(OLED_COLUMNS, OLED_PAGES, 0, 0, 0);

    i2c_master_init();
    ssd1306_init();
}
#endif // CONFIG_DISPLAY_SSD1306

#ifdef CONFIG_DISPLAY_M5STACK

esp_err_t lv_show_splash_screen()
{
    lv_img_set_src(lv_screens.splash.scr, &splash);
    lv_scr_load(lv_screens.splash.scr);
    return ESP_OK;
}

esp_err_t lv_show_beacon_screen(int idx)
{
    char buffer[32];
    uint8_t battery_level;

    lv_label_set_text_fmt(lv_screens.beacon_details.name, "%s", ble_beacons[idx].beacon_data.name);
    if (is_beacon_idx_active(idx) && (ble_beacons[idx].adv_data.last_seen != 0)){
        lv_label_set_text_fmt(lv_screens.beacon_details.temp_hum, "%4.1f°C  %3.0f%%",
            ble_beacons[idx].adv_data.temp, ble_beacons[idx].adv_data.humidity);

        battery_level = battery_level_in_percent(ble_beacons[idx].adv_data.battery);
        if(battery_level < 20){
            sprintf(buffer, "%s", LV_SYMBOL_BATTERY_EMPTY);
        } else if(battery_level < 40){
            sprintf(buffer, "%s", LV_SYMBOL_BATTERY_1);
        } else if(battery_level < 60){
            sprintf(buffer, "%s", LV_SYMBOL_BATTERY_2);
        } else if(battery_level < 80){
            sprintf(buffer, "%s", LV_SYMBOL_BATTERY_3);
        } else {
            sprintf(buffer, "%s", LV_SYMBOL_BATTERY_FULL);
        }
        lv_label_set_text_fmt(lv_screens.beacon_details.symbols.symbol_battery, "%s", buffer);
    } else {
        lv_label_set_text(lv_screens.beacon_details.temp_hum, "   -  °C    -  %H");
        lv_label_set_text_fmt(lv_screens.beacon_details.symbols.symbol_battery, "");
    }

    lv_label_set_text_fmt(lv_screens.beacon_details.symbols.symbol_eye, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE));

    lv_label_set_text_fmt(lv_screens.beacon_details.buttons.label1, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN));

    lv_label_set_text_fmt(lv_screens.beacon_details.buttons.label2, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_TRASH : ""));


    lv_label_set_text_fmt(lv_screens.beacon_details.pagenum.pagenum, "%d/%d", idx + 1, CONFIG_BLE_DEVICE_COUNT_USE);

    lv_scr_load(lv_screens.beacon_details.scr);

    return ESP_OK;
}

esp_err_t lv_show_last_seen_screen(uint8_t num_act_beac)
{
    char buffer1[32], buffer2[32];
    lv_obj_t * table = lv_screens.last_seen.table;
    int line;

    if (!num_act_beac) {
        display_status.lastseen_page_to_show = 1;
        lv_table_set_row_cnt(table, 2);
        line = 1;
        lv_table_set_cell_value(table, line, 0, "no active Beacons");
        lv_table_set_cell_value(table, line, 1, "");
        lv_table_set_cell_value(table, line, 2, "");
    } else {
        int skip = (display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
        int num_rows = 1 + MIN(num_act_beac - skip, 5);
        lv_table_set_row_cnt(table, num_rows);

        line = 1;
        for (int i = 0; i < CONFIG_BLE_DEVICE_COUNT_USE; i++)        {
            if (is_beacon_idx_active(i))            {
                if (skip)                {
                    skip--;
                } else {
                    bool never_seen = (ble_beacons[i].adv_data.last_seen == 0);
                    lv_table_set_cell_value(table, line, 0, ble_beacons[i].beacon_data.name);
                    if (never_seen){
                        lv_table_set_cell_value(table, line, 1, "/");
                        lv_table_set_cell_value(table, line, 2, "/");
                    } else {
                        uint16_t last_seen_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.last_seen) / 1000000;
                        uint16_t mqtt_last_send_sec_gone = (esp_timer_get_time() - ble_beacons[i].adv_data.mqtt_last_send) / 1000000;
                        uint8_t h, m, s, hq, mq, sq;
                        convert_s_hhmmss(last_seen_sec_gone, &h, &m, &s);
                        convert_s_hhmmss(mqtt_last_send_sec_gone, &hq, &mq, &sq);
                        if (h > 99) {
                            snprintf(buffer1, 32, "%s", "seen >99h");
                            snprintf(buffer2, 32, "%s", "/");
                        } else {
                            snprintf(buffer1, 32, "%02d:%02d:%02d", h, m, s);
                            snprintf(buffer2, 32, "%02d:%02d:%02d", hq, mq, sq);
                        }
                        lv_table_set_cell_value(table, line, 1, buffer1);
                        lv_table_set_cell_value(table, line, 2, buffer2);
                    }
                    if (BEAC_PER_PAGE_LASTSEEN == line++) {
                        break;
                    }
                }
            }
        }
    }
    lv_label_set_text_fmt(lv_screens.last_seen.pagenum.pagenum, "%d/%d", display_status.lastseen_page_to_show, display_status.num_last_seen_pages);
    lv_scr_load(lv_screens.last_seen.scr);

    return ESP_OK;
}

esp_err_t lv_show_app_version_screen()
{
    char buffer[32], buffer2[32];
    int line;
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    tcpip_adapter_ip_info_t ipinfo;
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    lv_obj_t * table = lv_screens.app_version.table;

    line = 1;
    snprintf(buffer, 32, "%s", app_desc->version);
    lv_table_set_cell_value(table, line, 0, "App. version");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    snprintf(buffer, 32, "%s", app_desc->project_name);
    lv_table_set_cell_value(table, line, 0, "Project");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    lv_table_set_cell_value(table, line, 0, "IDF version");
    lv_table_set_cell_value(table, line, 1, app_desc->idf_ver);

    line++;
    lv_table_set_cell_value(table, line, 0, "MAC Addr");
    snprintf(buffer, 32, "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    lv_table_set_cell_value(table, line, 0, "IP Addr");
    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipinfo);
    sprintf(buffer, IPSTR, IP2STR(&ipinfo.ip));
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    lv_table_set_cell_value(table, line, 0, "Active");
    itoa(s_active_beacon_mask, buffer2, 2);
    int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
    if (!num_lead_zeros) {
        snprintf(buffer, 32, "%s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    } else {
        snprintf(buffer, 32, "%0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    }
    lv_table_set_cell_value(table, line, 1, buffer);

    lv_scr_load(lv_screens.app_version.scr);

    return ESP_OK;
}

esp_err_t lv_show_stats_screen()
{
    char buffer[32];
    int line;
    uint32_t uptime_sec = esp_timer_get_time() / 1000000;
    uint16_t up_d;
    uint8_t up_h, up_m, up_s;
    EventBits_t uxReturn;

    lv_obj_t * table = lv_screens.stats.table;

    line = 1;
    convert_s_ddhhmmss(uptime_sec, &up_d, &up_h, &up_m, &up_s);
    snprintf(buffer, 32, "%dd %2d:%02d:%02d", up_d, up_h, up_m, up_s);
    lv_table_set_cell_value(table, line, 0, "Uptime");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    snprintf(buffer, 32, "%d / %d", wifi_connections_count_connect, wifi_connections_count_disconnect);
    lv_table_set_cell_value(table, line, 0, "WiFi ok/fail");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    snprintf(buffer, 32, "%d / %d", mqtt_packets_send, mqtt_packets_fail);
    lv_table_set_cell_value(table, line, 0, "MQTT ok/fail");
    lv_table_set_cell_value(table, line, 1, buffer);

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    bool mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;
    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

    line++;
    snprintf(buffer, 32, "%s", (wifi_connected ? "y" : "n"));
    lv_table_set_cell_value(table, line, 0, "WiFi conn.");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    snprintf(buffer, 32, "%s / %s", (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
    lv_table_set_cell_value(table, line, 0, "MQTT use/conn");
    lv_table_set_cell_value(table, line, 1, buffer);

    line++;
    snprintf(buffer, 32, "%s", (web_file_server_running ? "running" : "stopped"));
    lv_table_set_cell_value(table, line, 0, "Webserver");
    lv_table_set_cell_value(table, line, 1, buffer);

    lv_scr_load(lv_screens.stats.scr);

    return ESP_OK;
}

void lv_init_styles()
{
    // Styles for title text, big numbers, ...
    lv_style_copy(&style_screen, &lv_style_plain);
    style_screen.body.main_color = LV_COLOR_WHITE;
    style_screen.body.grad_color = LV_COLOR_WHITE;
    style_screen.text.color = LV_COLOR_GRAY;

    lv_style_copy(&style_title, &style_screen);
    style_title.text.font = &m5stack_22_font_symbol;

    lv_style_copy(&style_bigvalues, &style_screen);
    style_bigvalues.text.font = &m5stack_48_font_symbol;

    // table style normal cell
    lv_style_copy(&style_cell1, &lv_style_plain);
    style_cell1.body.border.width = 1;
    style_cell1.body.border.color = LV_COLOR_GRAY;
    style_cell1.body.padding.top = 2;
    style_cell1.body.padding.bottom = 2;

    // table style header cell
    lv_style_copy(&style_cell2, &lv_style_plain);
    style_cell2.body.border.width = 1;
    style_cell2.body.border.color = LV_COLOR_BLACK;
    style_cell2.body.main_color = LV_COLOR_SILVER;
    style_cell2.body.grad_color = LV_COLOR_SILVER;
    style_cell2.body.padding.top = 2;
    style_cell2.body.padding.bottom = 2;
}


void lv_init_screens()
{
    lv_obj_t * scr;

    lv_init_styles();

    // Splash screen
    lv_screens.splash.scr = lv_img_create(NULL, NULL);
    lv_img_cache_invalidate_src(NULL);

    // Beacon details screen
    lv_screens.beacon_details.scr = lv_obj_create(NULL, NULL);
    lv_obj_set_style(lv_screens.beacon_details.scr, &style_screen);
    scr = lv_screens.beacon_details.scr;

    // - content
    lv_screens.beacon_details.name =  lv_label_create(scr, NULL);
    lv_obj_set_style(lv_screens.beacon_details.name, &style_title);
    lv_obj_align(lv_screens.beacon_details.name, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
    lv_obj_set_auto_realign(lv_screens.beacon_details.name, true);

    lv_screens.beacon_details.temp_hum = lv_label_create(scr, NULL);
    lv_obj_set_style(lv_screens.beacon_details.temp_hum, &style_bigvalues);
    lv_obj_align(lv_screens.beacon_details.temp_hum, lv_screens.beacon_details.name, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_obj_set_auto_realign(lv_screens.beacon_details.temp_hum, true);

    // - buttons
    lv_screens.beacon_details.buttons.label1 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.buttons.label1, NULL, LV_ALIGN_IN_BOTTOM_MID, -95, 0);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label1, true);

    lv_screens.beacon_details.buttons.label2 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.buttons.label2, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label2, true);

    lv_screens.beacon_details.buttons.label3 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.beacon_details.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label3, true);

    // - symbols
    lv_screens.beacon_details.symbols.symbol_eye = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_eye, NULL, LV_ALIGN_IN_TOP_LEFT, 5, 5);
    lv_obj_set_auto_realign(lv_screens.beacon_details.symbols.symbol_eye, true);

    lv_screens.beacon_details.symbols.symbol_battery = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_battery, lv_screens.beacon_details.symbols.symbol_eye, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_auto_realign(lv_screens.beacon_details.symbols.symbol_battery, true);

    // - pagenum
    lv_screens.beacon_details.pagenum.pagenum = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.beacon_details.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, -5, 5);
    lv_obj_set_auto_realign(lv_screens.beacon_details.pagenum.pagenum, true);

    // Last seen screen
    lv_screens.last_seen.scr =  lv_obj_create(NULL, NULL);
    lv_obj_set_style(lv_screens.last_seen.scr, &style_screen);
    scr = lv_screens.last_seen.scr;

    lv_screens.last_seen.title = lv_label_create(scr, NULL);
    lv_obj_set_style(lv_screens.last_seen.title, &style_title);
    lv_label_set_text(lv_screens.last_seen.title, "Last seen/MQTT send");
    lv_obj_align(lv_screens.last_seen.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
    lv_obj_set_auto_realign(lv_screens.last_seen.title, true);

    lv_screens.last_seen.table = lv_table_create(scr, NULL);
    lv_obj_t * table = lv_screens.last_seen.table;

    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &style_cell1);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &style_cell2);
    lv_table_set_style(table, LV_TABLE_STYLE_BG, &lv_style_transp_tight);
    lv_table_set_col_cnt(table, 3);
    lv_table_set_row_cnt(table, 6);
    lv_obj_align(table, lv_screens.last_seen.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 2, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);
    lv_table_set_cell_type(table, 0, 2, 2);

    lv_table_set_cell_value(table, 0, 0, "Name");
    lv_table_set_cell_value(table, 0, 1, "Last Seen");
    lv_table_set_cell_value(table, 0, 2, "Last Send");

    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.last_seen.buttons.label3 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.last_seen.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.last_seen.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.last_seen.buttons.label3, true);

    // - pagenum
    lv_screens.last_seen.pagenum.pagenum = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.last_seen.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, -5, 5);
    lv_obj_set_auto_realign(lv_screens.last_seen.pagenum.pagenum, true);

    // App version screen
    lv_screens.app_version.scr = lv_obj_create(NULL, NULL);
    lv_obj_set_style(lv_screens.app_version.scr, &style_screen);
    scr = lv_screens.app_version.scr;

    lv_screens.app_version.title = lv_label_create(scr, NULL);
    lv_obj_set_style(lv_screens.app_version.title, &style_title);
    lv_label_set_text(lv_screens.app_version.title, "App. version + Status");
    lv_obj_align(lv_screens.app_version.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
    lv_obj_set_auto_realign(lv_screens.app_version.title, true);

    lv_screens.app_version.table = lv_table_create(scr, NULL);
    table = lv_screens.app_version.table;

    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &style_cell1);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &style_cell2);
    lv_table_set_style(table, LV_TABLE_STYLE_BG, &lv_style_transp_tight);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 7);
    lv_table_set_col_width(table, 0, 100);
    lv_table_set_col_width(table, 1, 200);
    lv_obj_align(table, lv_screens.app_version.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);

    lv_table_set_cell_value(table, 0, 0, "Field");
    lv_table_set_cell_value(table, 0, 1, "Value");

    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.app_version.buttons.label3 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.app_version.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.app_version.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.app_version.buttons.label3, true);

    // Stats screen
    lv_screens.stats.scr = lv_obj_create(NULL, NULL);
    lv_obj_set_style(lv_screens.stats.scr, &style_screen);
    scr = lv_screens.stats.scr;

    lv_screens.stats.title = lv_label_create(scr, NULL);
    lv_obj_set_style(lv_screens.stats.title, &style_title);
    lv_label_set_text(lv_screens.stats.title, "Statistics");
    lv_obj_align(lv_screens.stats.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
    lv_obj_set_auto_realign(lv_screens.stats.title, true);

    lv_screens.stats.table = lv_table_create(scr, NULL);
    table = lv_screens.stats.table;

    lv_table_set_style(table, LV_TABLE_STYLE_CELL1, &style_cell1);
    lv_table_set_style(table, LV_TABLE_STYLE_CELL2, &style_cell2);
    lv_table_set_style(table, LV_TABLE_STYLE_BG, &lv_style_transp_tight);
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 7);
    lv_table_set_col_width(table, 0, 140);
    lv_table_set_col_width(table, 1, 160);
    lv_obj_align(table, lv_screens.stats.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);

    lv_table_set_cell_value(table, 0, 0, "Field");
    lv_table_set_cell_value(table, 0, 1, "Value");

    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.stats.buttons.label2 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.stats.buttons.label2, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_label_set_text(lv_screens.stats.buttons.label2, LV_SYMBOL_TRASH);
    lv_obj_set_auto_realign(lv_screens.stats.buttons.label2, true);

    lv_screens.stats.buttons.label3 = lv_label_create(scr, NULL);
    lv_obj_align(lv_screens.stats.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.stats.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.stats.buttons.label3, true);
}

#endif // CONFIG_DISPLAY_M5STACK


esp_err_t display_update(void* _canvas, void * _canvas_message)
{
    esp_err_t ret;
    // char buffer[128], buffer2[32];
    EventBits_t uxReturn;
    UNUSED(uxReturn);

#ifdef CONFIG_DISPLAY_SSD1306
    ssd1306_canvas_t *canvas = (ssd1306_canvas_t *) _canvas;
    ssd1306_canvas_t *canvas_message = (ssd1306_canvas_t *) _canvas_message;
#endif
    // ESP_LOGD(TAG, "display_update >, run_periodic_timer %d, run_idle_timer_touch %d, periodic_timer_is_running %d, display_update current_screen %d, screen_to_show %d",
    //     run_periodic_timer, run_idle_timer_touch, periodic_timer_running, display_status.current_screen, display_status.screen_to_show);

    display_update_check_timer();
    if (display_update_check_display_off()){
        // display switched off, leave function
        return ESP_OK;
    }

    ESP_LOGD(TAG, "display_update display_message %d, display_message_is_shown %d", display_status.display_message, display_status.display_message_is_shown);
    if(display_status.display_message){
        if(display_status.display_message_is_shown && !display_message_content.need_refresh){
            // SOLL: anzeigen, IST: wird gezeigt
            return ESP_OK;
        } else {
            // SOLL: anzeigen, IST: wird nicht gezeigt
            display_status.display_message_is_shown = true;
            display_message_content.need_refresh = false;
#ifdef CONFIG_DISPLAY_SSD1306
            update_display_message(canvas_message);
            return ssd1306_refresh_gram(canvas_message);
#elif CONFIG_DISPLAY_M5STACK
            update_display_message_m5stack(true);
            return ESP_OK;
#endif
        }
    } else {
        if(display_status.display_message_is_shown){
            // SOLL: nicht anzeigen, IST: wird gezeigt
            display_status.current_screen = UNKNOWN_SCREEN;
            display_status.display_message_is_shown = false;
#ifdef CONFIG_DISPLAY_M5STACK
            update_display_message_m5stack(false);
#endif
        } else {
            // SOLL: nicht anzeigen, IST: nicht gezeigt
        }
    }

    ESP_LOGD(TAG, "display_update current_screen %d, screen_to_show %d", display_status.current_screen, display_status.screen_to_show);

    switch (display_status.screen_to_show)
    {

    case SPLASH_SCREEN:
        ESP_LOGD(TAG, "display_update SPLASH_SCREEN current_screen %d, screen_to_show %d", display_status.current_screen, display_status.screen_to_show);
#ifdef CONFIG_DISPLAY_SSD1306
        ret = ssd1306_show_splash_screen(canvas);
#elif CONFIG_DISPLAY_M5STACK
        ret = lv_show_splash_screen();
#endif
        display_status.current_screen = display_status.screen_to_show;

        return ret;
        break;

    case BEACON_SCREEN:
    {
        int idx = display_status.beac_to_show;
        if ((display_status.current_screen != display_status.screen_to_show)
            || (display_status.current_beac != display_status.beac_to_show))
        {

#ifdef CONFIG_DISPLAY_SSD1306
            ret = ssd1306_show_beacon_screen(canvas, idx);
#elif CONFIG_DISPLAY_M5STACK
            ret = lv_show_beacon_screen(idx);
#endif
            display_status.current_screen = display_status.screen_to_show;
            return ret;
        }
        else
        {
            ESP_LOGD(TAG, "display_update: not current screen to udate, exit");
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

#ifdef CONFIG_DISPLAY_SSD1306
            ret = ssd1306_show_last_seen_screen(canvas, num_act_beac);
#elif CONFIG_DISPLAY_M5STACK
            ret = lv_show_last_seen_screen(num_act_beac);
#endif
        display_status.current_screen = display_status.screen_to_show;

        return ret;
        break;
    }

    case APPVERSION_SCREEN:
    {
#ifdef CONFIG_DISPLAY_SSD1306
        ret = ssd1306_show_app_version_screen(canvas);
#elif CONFIG_DISPLAY_M5STACK
        ret = lv_show_app_version_screen();
#endif
        display_status.current_screen = display_status.screen_to_show;
        return ret;
    }
    break;

    case STATS_SCREEN:
    {
#ifdef CONFIG_DISPLAY_SSD1306
        ret = ssd1306_show_stats_screen(canvas);
#elif CONFIG_DISPLAY_M5STACK
        ret = lv_show_stats_screen();
#endif
        display_status.current_screen = display_status.screen_to_show;
        return ret;
    }
    break;

    default:
        ESP_LOGE(TAG, "unhandled display_update screen");
        break;
    }

    ESP_LOGE(TAG, "display_update: this line should not be reached");
    return ESP_FAIL;
}

void display_task(void *pvParameters)
{
    const esp_timer_create_args_t oneshot_display_message_timer_args = {
        .callback = &oneshot_display_message_timer_callback,
        .name     = "oneshot_display_message"
    };

    EventBits_t uxBits;
    UNUSED(uxBits);

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_display_message_timer_args, &oneshot_display_message_timer));

    while (1)
    {
        uxBits = xEventGroupWaitBits(s_values_evg, UPDATE_DISPLAY, pdTRUE, pdFALSE, portMAX_DELAY);
#ifdef CONFIG_DISPLAY_SSD1306
        display_update((void *)display_canvas, (void *)display_canvas_message);
#elif CONFIG_DISPLAY_M5STACK
        display_update(NULL, NULL);
#endif
    }
    vTaskDelete(NULL);
}
