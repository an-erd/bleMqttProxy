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

#include "lvgl/lvgl.h"
#include "lvgl_tft/disp_driver.h"

static const char *TAG = "display";

EventGroupHandle_t s_values_evg;
extern EventGroupHandle_t wifi_evg;
extern uint16_t wifi_connections_count_connect;
extern uint16_t wifi_connections_count_disconnect;
extern uint16_t wifi_ap_connections;
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

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
#include "ili9341.h"
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
#include "sh1107.h"
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306
#include "ssd1306.h"
#endif

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
LV_IMG_DECLARE(splash);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
LV_IMG_DECLARE(splash_oled);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
LV_IMG_DECLARE(splash_160x80);
#endif

// Styles for the screens
static lv_style_t style_screen;         // Overall page style
static lv_style_t style_title;          // Headline of the page
static lv_style_t style_bigvalues;      // Big center values (e.g. temperature/humidity)
static lv_style_t style_text;           // Normal text
static lv_style_t style_symbols_top;    // symbols on top of screen (battery, eye symbol)
static lv_style_t style_symbols_bottom; // symbols on bottom of screen (button actions)
static lv_style_t style_pagenum;        // pagenum font
static lv_style_t style_cell_bg;        // Table style, bg
static lv_style_t style_cell1;          // Table style, content cells
static lv_style_t style_cell2;          // Table style, header cells

lv_screens_t lv_screens;

LV_FONT_DECLARE(oled_9_font_symbol);
LV_FONT_DECLARE(lv_font_montserrat_9);
LV_FONT_DECLARE(lv_font_montserrat_10);
// LV_FONT_DECLARE(oled_12_font_symbol);
// LV_FONT_DECLARE(oled_16_font_symbol);
// LV_FONT_DECLARE(m5stack_16_font_symbol);
// LV_FONT_DECLARE(m5stack_22_font_symbol);
// LV_FONT_DECLARE(m5stack_36_font_symbol);
// LV_FONT_DECLARE(m5stack_48_font_symbol);

#if CONFIG_DEVICE_M5STACK==1
#define X_BUTTON_A	    65          // display button x position (for center of button)
#define X_BUTTON_B	    160
#define X_BUTTON_C	    255
#endif

void oneshot_display_message_timer_callback(void* arg)
{
    ESP_LOGD(TAG, "oneshot_display_message_timer_callback");
    display_message_stop_show();
}

void oneshot_display_message_timer_start()
{
    assert(oneshot_display_message_timer != NULL);
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

void update_display_message(bool show)
{
    static bool already_created = false;
    static bool is_showing = false;
    static lv_style_t modal_style, box_style, text_style1, text_style2;
    static lv_obj_t *mbox_obj, *mbox1;
    static lv_obj_t *text_title, *text_message, *text_comment, *text_action;
    static lv_obj_t *current_scr = NULL;

    ESP_LOGD(TAG, "update_display_message >, already_created %d, show %d, is_showing %d", already_created, show, is_showing);

    static const char * btns[] ={"Toggle", ""};

    UNUSED(current_scr);
    UNUSED(btns);
    UNUSED(text_action);
    UNUSED(text_comment);
    UNUSED(mbox1);
    UNUSED(text_style1);
    UNUSED(text_style2);
    UNUSED(text_title);
    UNUSED(box_style);
    UNUSED(modal_style);

    if(show){
        if(!already_created){
            lv_style_copy(&text_style1, &style_title);
            lv_style_copy(&text_style2, &style_text);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
            // modal_style.body.main_color = modal_style.body.grad_color = LV_COLOR_WHITE;
	    	// modal_style.body.opa = LV_OPA_50;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
    		lv_style_set_bg_color(&modal_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    		lv_style_set_bg_grad_color(&modal_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
            lv_style_set_text_color(&text_style1, LV_STATE_DEFAULT, LV_COLOR_WHITE);
            lv_style_set_text_color(&text_style2, LV_STATE_DEFAULT, LV_COLOR_WHITE);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    		lv_style_set_bg_color(&modal_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    		lv_style_set_bg_grad_color(&modal_style, LV_STATE_DEFAULT, LV_COLOR_BLACK);
            lv_style_set_text_color(&text_style1, LV_STATE_DEFAULT, LV_COLOR_RED);
            lv_style_set_text_color(&text_style2, LV_STATE_DEFAULT, LV_COLOR_GREEN);
#endif

            /* Create a base object for the modal background */
            mbox_obj = lv_obj_create(lv_scr_act(), NULL);
            lv_obj_add_style(mbox_obj, LV_OBJ_PART_MAIN, &modal_style);
            lv_obj_set_pos(mbox_obj, 0, 0);
            lv_obj_set_size(mbox_obj, LV_HOR_RES, LV_VER_RES);
            lv_obj_align(mbox_obj, NULL, LV_ALIGN_CENTER, 0, 0);

            text_title = lv_label_create(mbox_obj, NULL);
            text_message = lv_label_create(mbox_obj, NULL);
            text_comment = lv_label_create(mbox_obj, NULL);
            text_action = lv_label_create(mbox_obj, NULL);
            lv_obj_add_style(text_title, LV_OBJ_PART_MAIN, &text_style1);
            lv_obj_add_style(text_message, LV_OBJ_PART_MAIN, &text_style2);
            lv_obj_add_style(text_comment, LV_OBJ_PART_MAIN, &text_style2);
            lv_obj_add_style(text_action, LV_OBJ_PART_MAIN, &text_style2);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
            lv_obj_align(text_title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
            lv_obj_align(text_message, text_title, LV_ALIGN_IN_TOP_MID, 0, 5);
            lv_obj_align(text_comment, text_message, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
            lv_obj_align(text_action, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -5);

            lv_obj_set_auto_realign(text_title, true);
            lv_obj_set_auto_realign(text_message, true);
            lv_obj_set_auto_realign(text_comment, true);
            lv_obj_set_auto_realign(text_action, true);
#endif

            already_created = true;
        }

ESP_LOGD(TAG, "update_display_message >, set/update text");
        lv_label_set_text(text_title, display_message_content.title);
        lv_label_set_text(text_message, display_message_content.message);
        lv_label_set_text(text_comment, display_message_content.comment);
        lv_label_set_text(text_action, display_message_content.action);
        lv_obj_set_top(text_title, true);
        lv_obj_set_top(text_message, true);
        lv_obj_set_top(text_comment, true);
        lv_obj_set_top(text_action, true);
        is_showing = true;
    } else {
ESP_LOGD(TAG, "update_display_message check 1");
        if(is_showing){
ESP_LOGD(TAG, "update_display_message check 2");
            lv_obj_del(mbox_obj);
            already_created = false;
            is_showing = false;
        }
    }
ESP_LOGD(TAG, "update_display_message check <");
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
    if (turn_display_off){
        if (display_status.display_on){
            display_status.display_on = false;
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
            ili9341_sleep_in();
            ili9341_enable_backlight(false);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306
            ssd1306_sleep_in();
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
            sh1107_sleep_in();
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
            st7735s_sleep_in();
#endif
        }
        return true;
    } else {
        if (!display_status.display_on) {
            display_status.display_on = true;
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
            ili9341_sleep_out();
            ili9341_enable_backlight(true);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306
            ssd1306_sleep_out();
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
            sh1107_sleep_out();
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
            st7735s_sleep_out();
#endif
        }
    }

    return false;
}

esp_err_t lv_show_splash_screen()
{
    lv_scr_load(lv_screens.splash.scr);
    return ESP_OK;
}

esp_err_t lv_show_beacon_screen(int idx)
{
    char buffer[32];
    uint8_t battery_level;

    lv_label_set_text_fmt(lv_screens.beacon_details.name, "%s", ble_beacons[idx].beacon_data.name);
    if (is_beacon_idx_active(idx) && (ble_beacons[idx].adv_data.last_seen != 0)){
        snprintf(buffer, 32, "%4.1f°C  %3.0f%%",
            ble_beacons[idx].adv_data.temp, ble_beacons[idx].adv_data.humidity);
        lv_label_set_text(lv_screens.beacon_details.temp_hum, buffer);
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
        lv_label_set_text_fmt(lv_screens.beacon_details.battery, "Batt %4d mV", ble_beacons[idx].adv_data.battery);
        lv_label_set_text_fmt(lv_screens.beacon_details.rssi, "RSSI %3d dBm", ble_beacons[idx].adv_data.measured_power);
    } else {
        lv_label_set_text(lv_screens.beacon_details.temp_hum, "-  °C    -  %H");
        lv_label_set_text_fmt(lv_screens.beacon_details.symbols.symbol_battery, "");
        lv_label_set_text_fmt(lv_screens.beacon_details.battery, "Batt - mV");
        lv_label_set_text_fmt(lv_screens.beacon_details.rssi, "RSSI - dBm");
    }

    lv_label_set_text_fmt(lv_screens.beacon_details.symbols.symbol_eye, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_EYE_OPEN : LV_SYMBOL_EYE_CLOSE));

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_label_set_text_fmt(lv_screens.beacon_details.buttons.label1, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN));
    lv_label_set_text_fmt(lv_screens.beacon_details.buttons.label2, "%s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_TRASH : ""));
    lv_label_set_text(lv_screens.beacon_details.buttons.label3, LV_SYMBOL_RIGHT);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_label_set_text_fmt(lv_screens.beacon_details.buttons.label3, "(%s) %s",
        (is_beacon_idx_active(idx) ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN), LV_SYMBOL_RIGHT);
#endif

    lv_label_set_text_fmt(lv_screens.beacon_details.pagenum.pagenum, "%d/%d", idx + 1, CONFIG_BLE_DEVICE_COUNT_USE);
    lv_scr_load(lv_screens.beacon_details.scr);

    return ESP_OK;
}

esp_err_t lv_show_last_seen_screen(uint8_t num_act_beac)
{
    char buffer1[32], buffer2[32];
    lv_obj_t * table = lv_screens.last_seen.table;
    int line = 0, num_rows = 0;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    line = 1;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    line = 0;
#endif

    if (!num_act_beac) {
        display_status.lastseen_page_to_show = 1;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
        lv_table_set_row_cnt(table, 2);
        lv_table_set_cell_value(table, line, 0, "no active Beacons");
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
        lv_table_set_row_cnt(table, 1);
        lv_table_set_cell_value(table, line, 0, "no active Beacons");
#endif
        lv_table_set_cell_value(table, line, 1, "");
        lv_table_set_cell_value(table, line, 2, "");
    } else {
        int skip = (display_status.lastseen_page_to_show - 1) * BEAC_PER_PAGE_LASTSEEN;
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
        num_rows = 1 + MIN(num_act_beac - skip, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
        num_rows = 0 + MIN(num_act_beac - skip, 5);
#endif
        lv_table_set_row_cnt(table, num_rows);

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
    int line = 1, col = 1;
    const esp_app_desc_t *app_desc = esp_ota_get_app_description();
    tcpip_adapter_ip_info_t ipinfo;
    uint8_t mac[6];
    ESP_ERROR_CHECK(esp_efuse_mac_get_default(mac));
    lv_obj_t * table = lv_screens.app_version.table;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_cell_value(table, 1, 0, "App. version");
    lv_table_set_cell_value(table, 2, 0, "Project");
    lv_table_set_cell_value(table, 3, 0, "IDF version");
    lv_table_set_cell_value(table, 4, 0, "MAC Addr");
    lv_table_set_cell_value(table, 5, 0, "IP Addr");
    lv_table_set_cell_value(table, 6, 0, "Active");
    line = 1;
    col = 1;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    line = 0;
    col = 0;
#endif

    lv_table_set_cell_value(table, line++, col, app_desc->version);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_table_set_cell_value(table, line++, col, app_desc->project_name);
#endif
    lv_table_set_cell_value(table, line++, col, app_desc->idf_ver);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "MAC: %02X:%02X:%02X:%02X:%02X:%02X",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

    tcpip_adapter_get_ip_info(TCPIP_ADAPTER_IF_STA, &ipinfo);
    snprintf_nowarn(buffer2, 32, IPSTR, IP2STR(&ipinfo.ip));
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf_nowarn(buffer, 32, "%s", buffer2);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf_nowarn(buffer, 32, "IP: %s", buffer2);
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

    itoa(s_active_beacon_mask, buffer2, 2);
    int num_lead_zeros = CONFIG_BLE_DEVICE_COUNT_USE - strlen(buffer2);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    if (!num_lead_zeros) {
        snprintf_nowarn(buffer, 32, "%s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    } else {
        snprintf_nowarn(buffer, 32, "%0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    }
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    if (!num_lead_zeros) {
        snprintf_nowarn(buffer, 32, "Active: %s (%d..1)", buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    } else {
        snprintf_nowarn(buffer, 32, "Active: %0*d%s (%d..1)", num_lead_zeros, 0, buffer2, CONFIG_BLE_DEVICE_COUNT_USE);
    }
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

    lv_scr_load(lv_screens.app_version.scr);

    return ESP_OK;
}

esp_err_t lv_show_stats_screen()
{
    char buffer[32];
    int line = 1, col = 1;
    uint32_t uptime_sec = esp_timer_get_time() / 1000000;
    uint16_t up_d;
    uint8_t up_h, up_m, up_s;
    EventBits_t uxReturn;
    lv_obj_t * table = lv_screens.stats.table;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_cell_value(table, 1, 0, "Uptime");
    lv_table_set_cell_value(table, 2, 0, "WiFi ok/fail");
    lv_table_set_cell_value(table, 3, 0, "MQTT ok/fail");
    lv_table_set_cell_value(table, 4, 0, "WiFi conn.");
    lv_table_set_cell_value(table, 5, 0, "MQTT use/conn");
    lv_table_set_cell_value(table, 6, 0, "Webserver");
    line = 1;
    col = 1;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    line = 0;
    col = 0;
#endif


    convert_s_ddhhmmss(uptime_sec, &up_d, &up_h, &up_m, &up_s);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%dd %2d:%02d:%02d", up_d, up_h, up_m, up_s);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "Uptime: %dd %2d:%02d:%02d", up_d, up_h, up_m, up_s);
#endif
    lv_table_set_cell_value(table, line++, col, buffer);


#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%d / %d", wifi_connections_count_connect, wifi_connections_count_disconnect);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "WiFi ok/fail: %d / %d", wifi_connections_count_connect, wifi_connections_count_disconnect);
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%d / %d", mqtt_packets_send, mqtt_packets_fail);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "MQTT ok/fail: %d / %d", mqtt_packets_send, mqtt_packets_fail);
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

    uxReturn = xEventGroupWaitBits(mqtt_evg, MQTT_CONNECTED_BIT, false, true, 0);
    bool mqtt_connected = uxReturn & MQTT_CONNECTED_BIT;
    uxReturn = xEventGroupWaitBits(wifi_evg, WIFI_CONNECTED_BIT, false, true, 0);
    bool wifi_connected = uxReturn & WIFI_CONNECTED_BIT;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%s", (wifi_connected ? "y" : "n"));
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "WiFi: %s, AP: %d, WebSrv: %s",
        (wifi_connected ? "y" : "n"), wifi_ap_connections, (web_file_server_running ? "y" : "n"));
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%s / %s", (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    snprintf(buffer, 32, "MQTT conf/conn: %s / %s", (CONFIG_USE_MQTT ? "y" : "n"), (mqtt_connected ? "y" : "n"));
#endif
    lv_table_set_cell_value(table, line++, col, buffer);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    snprintf(buffer, 32, "%s", (web_file_server_running ? "running" : "stopped"));
    lv_table_set_cell_value(table, line++, col, buffer);
#endif

    lv_scr_load(lv_screens.stats.scr);

    return ESP_OK;
}

void lv_init_styles()
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
    lv_style_set_bg_grad_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_WHITE);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_style_set_text_color(&style_screen, LV_STATE_DEFAULT, LV_COLOR_GRAY); // LV_COLOR_BLACK;
#endif

    lv_style_copy(&style_title,             &style_screen);
    lv_style_copy(&style_bigvalues,         &style_screen);
    lv_style_copy(&style_text,              &style_screen);
    lv_style_copy(&style_symbols_top,       &style_screen);
    lv_style_copy(&style_symbols_bottom,    &style_screen);
    lv_style_copy(&style_pagenum,           &style_screen);

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_style_set_text_font(&style_title,            LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SUBTITLE);
    lv_style_set_text_font(&style_bigvalues,        LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_style_set_text_font(&style_text,             LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_NORMAL);
    lv_style_set_text_font(&style_symbols_top,      LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
    lv_style_set_text_font(&style_symbols_bottom,   LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
    lv_style_set_text_font(&style_pagenum,          LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SMALL);
#else
    lv_style_set_text_font(&style_title,            LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_SUBTITLE);
    lv_style_set_text_font(&style_bigvalues,        LV_STATE_DEFAULT, LV_THEME_DEFAULT_FONT_TITLE);
    lv_style_set_text_font(&style_text,             LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_font(&style_symbols_top,      LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_font(&style_symbols_bottom,   LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_font(&style_pagenum,          LV_STATE_DEFAULT, &lv_font_montserrat_9);
#endif

    lv_style_set_text_color(&style_title,       LV_STATE_DEFAULT, LV_COLOR_RED);
    lv_style_set_text_color(&style_text,        LV_STATE_DEFAULT, LV_COLOR_BLUE);
    lv_style_set_text_color(&style_symbols_top, LV_STATE_DEFAULT, LV_COLOR_GREEN);


    lv_style_init(&style_cell_bg);
    lv_style_init(&style_cell1);
    lv_style_init(&style_cell2);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_style_set_border_width(&style_cell1, LV_STATE_DEFAULT, 1);
    lv_style_set_border_color(&style_cell1, LV_STATE_DEFAULT, LV_COLOR_GRAY);
    lv_style_set_pad_top(&style_cell1, LV_STATE_DEFAULT, 2);
    lv_style_set_pad_bottom(&style_cell1, LV_STATE_DEFAULT, 2);

    lv_style_set_border_width(&style_cell2, LV_STATE_DEFAULT, 1);
    lv_style_set_border_color(&style_cell2, LV_STATE_DEFAULT, LV_COLOR_BLACK);
    lv_style_set_bg_color(&style_cell2, LV_STATE_DEFAULT, LV_COLOR_SILVER);
    lv_style_set_bg_grad_color(&style_cell2, LV_STATE_DEFAULT, LV_COLOR_SILVER);
    lv_style_set_pad_top(&style_cell2, LV_STATE_DEFAULT, 2);
    lv_style_set_pad_bottom(&style_cell2, LV_STATE_DEFAULT, 2);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_style_set_border_width(&style_cell_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_top(&style_cell_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_bottom(&style_cell_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_left(&style_cell_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_right(&style_cell_bg, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_inner(&style_cell_bg, LV_STATE_DEFAULT, 0);

    lv_style_set_border_width(&style_cell1, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_top(&style_cell1, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_bottom(&style_cell1, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_left(&style_cell1, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_right(&style_cell1, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_inner(&style_cell1, LV_STATE_DEFAULT, 0);

    lv_style_set_border_width(&style_cell2, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_top(&style_cell2, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_bottom(&style_cell2, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_left(&style_cell2, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_right(&style_cell2, LV_STATE_DEFAULT, 0);
    lv_style_set_pad_inner(&style_cell2, LV_STATE_DEFAULT, 0);

    lv_style_set_text_font(&style_cell1, LV_STATE_DEFAULT, &lv_font_montserrat_10);
    lv_style_set_text_font(&style_cell2, LV_STATE_DEFAULT, &lv_font_montserrat_10);
#endif
}

void lv_init_screens()
{
    lv_obj_t * scr;

    lv_init_styles();

    // Splash screen
    lv_screens.splash.scr = lv_img_create(NULL, NULL);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_img_set_src(lv_screens.splash.scr, &splash);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
    lv_img_set_src(lv_screens.splash.scr, &splash_oled);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_img_set_src(lv_screens.splash.scr, &splash_160x80);
#endif

    lv_img_cache_invalidate_src(NULL);

    // Beacon details screen
    lv_screens.beacon_details.scr = lv_obj_create(NULL, NULL);
    lv_obj_add_style(lv_screens.beacon_details.scr, LV_OBJ_PART_MAIN, &style_screen);
    scr = lv_screens.beacon_details.scr;

    // - content
    lv_screens.beacon_details.name = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.name, LV_OBJ_PART_MAIN, &style_title);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.beacon_details.name, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.beacon_details.name, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.beacon_details.name, true);

    lv_screens.beacon_details.temp_hum = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.temp_hum, LV_OBJ_PART_MAIN, &style_bigvalues);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.beacon_details.temp_hum, lv_screens.beacon_details.name, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.beacon_details.temp_hum, lv_screens.beacon_details.name, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.beacon_details.temp_hum, true);

    lv_screens.beacon_details.battery = lv_label_create(scr, NULL);
    lv_screens.beacon_details.rssi = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.battery, LV_OBJ_PART_MAIN, &style_text);
    lv_obj_add_style(lv_screens.beacon_details.rssi, LV_OBJ_PART_MAIN, &style_text);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_set_hidden(lv_screens.beacon_details.battery, true);
    lv_obj_set_hidden(lv_screens.beacon_details.rssi, true);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.beacon_details.battery, lv_screens.beacon_details.temp_hum, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_align(lv_screens.beacon_details.rssi, lv_screens.beacon_details.battery, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.beacon_details.battery, true);
    lv_obj_set_auto_realign(lv_screens.beacon_details.rssi, true);

    // - buttons
    lv_screens.beacon_details.buttons.label1 = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.buttons.label1, LV_OBJ_PART_MAIN, &style_symbols_bottom);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label1, true);

    lv_screens.beacon_details.buttons.label2 = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.buttons.label2, LV_OBJ_PART_MAIN, &style_symbols_bottom);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label2, true);

    lv_screens.beacon_details.buttons.label3 = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.buttons.label3, LV_OBJ_PART_MAIN, &style_symbols_bottom);
    lv_obj_set_auto_realign(lv_screens.beacon_details.buttons.label3, true);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.beacon_details.buttons.label1, NULL, LV_ALIGN_IN_BOTTOM_MID, -95, 0);
    lv_obj_align(lv_screens.beacon_details.buttons.label2, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_obj_align(lv_screens.beacon_details.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_set_hidden(lv_screens.beacon_details.buttons.label1, true);
    lv_obj_set_hidden(lv_screens.beacon_details.buttons.label2, true);
    lv_obj_align(lv_screens.beacon_details.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
#endif

    // - symbols
    lv_screens.beacon_details.symbols.symbol_eye = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.symbols.symbol_eye, LV_OBJ_PART_MAIN, &style_symbols_top);
    lv_obj_set_auto_realign(lv_screens.beacon_details.symbols.symbol_eye, true);

    lv_screens.beacon_details.symbols.symbol_battery = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.symbols.symbol_battery, LV_OBJ_PART_MAIN, &style_symbols_top);
    lv_obj_set_auto_realign(lv_screens.beacon_details.symbols.symbol_battery, true);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_eye, NULL, LV_ALIGN_IN_TOP_LEFT, 5, 5);
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_battery, lv_screens.beacon_details.symbols.symbol_eye, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_eye, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_align(lv_screens.beacon_details.symbols.symbol_battery, lv_screens.beacon_details.symbols.symbol_eye, LV_ALIGN_OUT_RIGHT_MID, 1, 0);
#endif

    // - pagenum
    lv_screens.beacon_details.pagenum.pagenum = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.beacon_details.pagenum.pagenum, LV_OBJ_PART_MAIN, &style_pagenum);
    lv_obj_set_auto_realign(lv_screens.beacon_details.pagenum.pagenum, true);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.beacon_details.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, -5, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.beacon_details.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
#endif

    // Last seen screen
    lv_screens.last_seen.scr = lv_obj_create(NULL, NULL);
    lv_obj_add_style(lv_screens.last_seen.scr, LV_OBJ_PART_MAIN, &style_screen);
    scr = lv_screens.last_seen.scr;

    lv_screens.last_seen.title = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.last_seen.title, LV_OBJ_PART_MAIN, &style_title);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_label_set_text(lv_screens.last_seen.title, "Last seen/MQTT send");
    lv_obj_align(lv_screens.last_seen.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_label_set_text(lv_screens.last_seen.title, "Last seen/send");
    lv_obj_align(lv_screens.last_seen.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.last_seen.title, true);

    lv_screens.last_seen.table = lv_table_create(scr, NULL);
    lv_obj_t * table = lv_screens.last_seen.table;
    lv_obj_add_style(table, LV_TABLE_PART_BG, &style_cell_bg);
    lv_obj_add_style(lv_screens.last_seen.table, LV_TABLE_PART_CELL1, &style_cell1);
    lv_obj_add_style(lv_screens.last_seen.table, LV_TABLE_PART_CELL2, &style_cell2);
    lv_table_set_col_cnt(table, 3);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_row_cnt(table, 6);
    lv_obj_align(table, lv_screens.last_seen.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
    lv_table_set_row_cnt(table, 5);
    lv_table_set_col_width(table, 0, 40);
    lv_table_set_col_width(table, 1, 43);
    lv_table_set_col_width(table, 2, 43);
    lv_obj_align(table, lv_screens.last_seen.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_table_set_row_cnt(table, 5);
    lv_table_set_col_width(table, 0, 50);
    lv_table_set_col_width(table, 1, 50);
    lv_table_set_col_width(table, 2, 50);
    lv_obj_align(table, lv_screens.last_seen.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#endif

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 2, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);
    lv_table_set_cell_type(table, 0, 2, 2);
    lv_table_set_cell_value(table, 0, 0, "Name");
    lv_table_set_cell_value(table, 0, 1, "Last Seen");
    lv_table_set_cell_value(table, 0, 2, "Last Send");
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
#endif
    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.last_seen.buttons.label3 = lv_label_create(scr, NULL);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_label_set_text(lv_screens.last_seen.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_align(lv_screens.last_seen.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_obj_set_auto_realign(lv_screens.last_seen.buttons.label3, true);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_set_hidden(lv_screens.last_seen.buttons.label3, true);
#endif
    // - pagenum
    lv_screens.last_seen.pagenum.pagenum = lv_label_create(scr, NULL);
    // lv_obj_set_style(lv_screens.last_seen.pagenum.pagenum, &style_pagenum);
    lv_obj_align(lv_screens.last_seen.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, -5, 5);
    lv_obj_set_auto_realign(lv_screens.last_seen.pagenum.pagenum, true);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.last_seen.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, -5, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.last_seen.pagenum.pagenum, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 0);
#endif

    // App version screen
    lv_screens.app_version.scr = lv_obj_create(NULL, NULL);
    lv_obj_add_style(lv_screens.app_version.scr, LV_OBJ_PART_MAIN, &style_screen);
    scr = lv_screens.app_version.scr;

    lv_screens.app_version.title = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.app_version.title, LV_OBJ_PART_MAIN, &style_title);
    lv_label_set_text(lv_screens.app_version.title, "App. version + Status");
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.app_version.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.app_version.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.app_version.title, true);

    lv_screens.app_version.table = lv_table_create(scr, NULL);
    table = lv_screens.app_version.table;
    lv_obj_add_style(table, LV_TABLE_PART_BG, &style_cell_bg);
    lv_obj_add_style(lv_screens.app_version.table, LV_TABLE_PART_CELL1, &style_cell1);
    lv_obj_add_style(lv_screens.app_version.table, LV_TABLE_PART_CELL2, &style_cell2);
    lv_table_set_col_cnt(table, 3);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 7);
    lv_table_set_col_width(table, 0, 100);
    lv_table_set_col_width(table, 1, 200);
    lv_obj_align(table, lv_screens.app_version.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_table_set_col_cnt(table, 1);
    lv_table_set_row_cnt(table, 5);
    lv_table_set_col_width(table, 0, 150);
    lv_obj_align(table, lv_screens.app_version.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#endif

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);
    lv_table_set_cell_value(table, 0, 0, "Field");
    lv_table_set_cell_value(table, 0, 1, "Value");
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
#endif
    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.app_version.buttons.label3 = lv_label_create(scr, NULL);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.app_version.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.app_version.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.app_version.buttons.label3, true);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_set_hidden(lv_screens.app_version.buttons.label3, true);
#endif

    // Stats screen
    lv_screens.stats.scr = lv_obj_create(NULL, NULL);
    lv_obj_add_style(lv_screens.stats.scr, LV_OBJ_PART_MAIN, &style_screen);
    scr = lv_screens.stats.scr;

    lv_screens.stats.title = lv_label_create(scr, NULL);
    lv_obj_add_style(lv_screens.stats.title, LV_OBJ_PART_MAIN, &style_title);
    lv_label_set_text(lv_screens.stats.title, "Statistics");
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.stats.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 5);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_align(lv_screens.stats.title, NULL, LV_ALIGN_IN_TOP_MID, 0, 0);
#endif
    lv_obj_set_auto_realign(lv_screens.stats.title, true);

    lv_screens.stats.table = lv_table_create(scr, NULL);
    table = lv_screens.stats.table;
    lv_obj_add_style(table, LV_TABLE_PART_BG, &style_cell_bg);
    lv_obj_add_style(lv_screens.stats.table, LV_TABLE_PART_CELL1, &style_cell1);
    lv_obj_add_style(lv_screens.stats.table, LV_TABLE_PART_CELL2, &style_cell2);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_col_cnt(table, 2);
    lv_table_set_row_cnt(table, 7);
    lv_table_set_col_width(table, 0, 140);
    lv_table_set_col_width(table, 1, 160);
    lv_obj_align(table, lv_screens.stats.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_table_set_col_cnt(table, 1);
    lv_table_set_row_cnt(table, 5);
    lv_table_set_col_width(table, 0, 150);
    lv_obj_align(table, lv_screens.stats.title, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
#endif

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_table_set_cell_align(table, 0, 0, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_align(table, 0, 1, LV_LABEL_ALIGN_CENTER);
    lv_table_set_cell_type(table, 0, 0, 2);
    lv_table_set_cell_type(table, 0, 1, 2);
    lv_table_set_cell_value(table, 0, 0, "Field");
    lv_table_set_cell_value(table, 0, 1, "Value");
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
#endif
    lv_obj_set_auto_realign(table, true);

    // - buttons
    lv_screens.stats.buttons.label2 = lv_label_create(scr, NULL);
    lv_screens.stats.buttons.label3 = lv_label_create(scr, NULL);
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_obj_align(lv_screens.stats.buttons.label2, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, 0);
    lv_label_set_text(lv_screens.stats.buttons.label2, LV_SYMBOL_TRASH);
    lv_obj_set_auto_realign(lv_screens.stats.buttons.label2, true);

    lv_obj_align(lv_screens.stats.buttons.label3, NULL, LV_ALIGN_IN_BOTTOM_MID, +95, 0);
    lv_label_set_text(lv_screens.stats.buttons.label3, LV_SYMBOL_RIGHT);
    lv_obj_set_auto_realign(lv_screens.stats.buttons.label3, true);
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107 || defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ST7735S
    lv_obj_set_hidden(lv_screens.stats.buttons.label2, true);
    lv_obj_set_hidden(lv_screens.stats.buttons.label3, true);
#endif

    ESP_LOGI(TAG, "lv_init_screens done");
}


esp_err_t display_update()
{
    esp_err_t ret;
    // char buffer[128], buffer2[32];
    EventBits_t uxReturn;
    UNUSED(uxReturn);

    // ESP_LOGD(TAG, "display_update >, run_periodic_timer %d, run_idle_timer_touch %d, periodic_timer_is_running %d, display_update current_screen %d, screen_to_show %d",
    //     run_periodic_timer, run_idle_timer_touch, periodic_timer_running, display_status.current_screen, display_status.screen_to_show);

    display_update_check_timer();
    if (display_update_check_display_off()){
        // display switched off, leave function
        return ESP_OK;
    }

    ESP_LOGD(TAG, "display_update display_message %d, display_message_is_shown %d", display_status.display_message, display_status.display_message_is_shown);
    if(display_status.display_message){
        display_status.display_message_is_shown = true;
        update_display_message(true);
        return ESP_OK;
    } else {
        update_display_message(false);
        display_status.display_message_is_shown = false;
        display_status.current_screen = UNKNOWN_SCREEN;
    }
    if(display_status.display_message){
        if(display_status.display_message_is_shown && !display_message_content.need_refresh){
            // SOLL: anzeigen, IST: wird gezeigt
            return ESP_OK;
        } else {
            // SOLL: anzeigen, IST: wird nicht gezeigt
            display_status.display_message_is_shown = true;
            display_message_content.need_refresh = false;
            update_display_message(true);
            return ESP_OK;
        }
    } else {
        if(display_status.display_message_is_shown){
            // SOLL: nicht anzeigen, IST: wird gezeigt
            display_status.current_screen = UNKNOWN_SCREEN;
            display_status.display_message_is_shown = false;
            ESP_LOGI(TAG, "display_update, display_message_is_shown = false");
            update_display_message(false);
        } else {
            // SOLL: nicht anzeigen, IST: nicht gezeigt
        }
    }

    ESP_LOGD(TAG, "display_update current_screen %d, screen_to_show %d", display_status.current_screen, display_status.screen_to_show);

    switch (display_status.screen_to_show)
    {

    case SPLASH_SCREEN:
        ret = lv_show_splash_screen();
        display_status.current_screen = display_status.screen_to_show;

        return ret;
        break;

    case BEACON_SCREEN:
    {
        int idx = display_status.beac_to_show;
        if ((display_status.current_screen != display_status.screen_to_show)
            || (display_status.current_beac != display_status.beac_to_show)) {
            ret = lv_show_beacon_screen(idx);
            display_status.current_screen = display_status.screen_to_show;
            return ret;
        } else {
            ESP_LOGD(TAG, "display_update: not current screen to udate, exit");
            return ESP_OK;
        }
        break;
    }

    case LASTSEEN_SCREEN:
    {
        uint8_t num_act_beac = num_active_beacon();
        display_status.num_last_seen_pages = num_act_beac / BEAC_PER_PAGE_LASTSEEN + (num_act_beac % BEAC_PER_PAGE_LASTSEEN ? 1 : 0) + (!num_act_beac ? 1 : 0);

        if (display_status.lastseen_page_to_show > display_status.num_last_seen_pages) {
            // due to deannouncment by "touching" the beacon - TODO
            display_status.lastseen_page_to_show = display_status.num_last_seen_pages;
        }
        ret = lv_show_last_seen_screen(num_act_beac);
        display_status.current_screen = display_status.screen_to_show;

        return ret;
        break;
    }

    case APPVERSION_SCREEN:
    {
        ret = lv_show_app_version_screen();
        display_status.current_screen = display_status.screen_to_show;
        return ret;
    }
    break;

    case STATS_SCREEN:
    {
        ret = lv_show_stats_screen();
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

void display_create_timer()
{
    const esp_timer_create_args_t oneshot_display_message_timer_args = {
        .callback = &oneshot_display_message_timer_callback,
        .name     = "oneshot_display_message"
    };

    ESP_ERROR_CHECK(esp_timer_create(&oneshot_display_message_timer_args, &oneshot_display_message_timer));
}

void update_display_task(lv_task_t * task)
{
    display_update();
}