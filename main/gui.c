#include "blemqttproxy.h"

#include "lvgl/lvgl.h"
#include "lvgl_helpers.h"

#include "gui.h"
#include "display.h"

static const char* TAG = "GUI";

static void IRAM_ATTR lv_tick_task(void* arg) {
	lv_tick_inc(portTICK_RATE_MS);
}

//Creates a semaphore to handle concurrent call to lvgl stuff
//If you wish to call *any* lvgl function from other threads/tasks
//you should lock on the very same semaphore!
SemaphoreHandle_t xGuiSemaphore;

void initialize_lv()
{
    lv_init();
    lvgl_driver_init();
    static lv_color_t buf1[DISP_BUF_SIZE];
#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    static lv_color_t buf2[DISP_BUF_SIZE];
#endif
    static lv_disp_buf_t disp_buf;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_ILI9341
    lv_disp_buf_init(&disp_buf, buf1, buf2, DISP_BUF_SIZE);
#else
    lv_disp_buf_init(&disp_buf, buf1, NULL, DISP_BUF_SIZE);
#endif

    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = disp_driver_flush;
    disp_drv.rounder_cb = disp_driver_rounder;

#if defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SSD1306
    disp_drv.set_px_cb = ssd1306_set_px_cb;
#elif defined CONFIG_LVGL_TFT_DISPLAY_CONTROLLER_SH1107
    disp_drv.set_px_cb = sh1107_set_px_cb;
#endif
    disp_drv.buffer = &disp_buf;
    lv_disp_drv_register(&disp_drv);
}

static void gui_prepare()
{
    xGuiSemaphore = xSemaphoreCreateMutex();

    initialize_lv();

    const esp_timer_create_args_t periodic_timer_args = {
            .callback = &lv_tick_task,
            /* name is optional, but may help identify the timer when debugging */
            .name = "periodic_gui"
    };
    static esp_timer_handle_t periodic_timer;
    ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));
    //On ESP32 it's better to create a periodic task instead of esp_register_freertos_tick_hook
    ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10*1000)); //10ms (expressed as microseconds)

    lv_init_screens();
}


void gui_task(void* pvParameters)
{
    display_create_timer();
    gui_prepare();
    ESP_LOGD(TAG, "gui_task, gui_prepare() done"); fflush(stdout);

    ESP_LOGI(TAG, "gui_task > uxTaskGetStackHighWaterMark '%d'", uxTaskGetStackHighWaterMark(NULL));
    lv_task_create(update_display_task, 100, LV_TASK_PRIO_MID, NULL);
    ESP_LOGI(TAG, "gui_task < uxTaskGetStackHighWaterMark '%d'", uxTaskGetStackHighWaterMark(NULL));

    while (1) {
        vTaskDelay(1);
        //Try to lock the semaphore, if success, call lvgl stuff
        if (xSemaphoreTake(xGuiSemaphore, (TickType_t)10) == pdTRUE) {
            lv_task_handler();
            xSemaphoreGive(xGuiSemaphore);
        }
    }
    vTaskDelete(NULL);
}

