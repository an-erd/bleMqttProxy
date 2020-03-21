
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "lvgl/lvgl.h"
#include "lv_testdisplay.h"

// Create the display objects
//
// 1 Screen Beacon Details
//      Label       Name
//      Label       Temperature
//      Label       Humidity
//      Label       Battery Level
//      Label       RSSI
//      Checkbox    Active
//      Label       Page Number
//      Option 1) List view similar to OLED display
//      Option 2) Nice view with Thermometer and Humidity Gauge
//
// 2 Screen Last Seen/Last send List
//      Multiple Lines
//          Label Name      Label Time last seen       Label Time Label last send
//      Label       Page Number
//
// 3 Screen App Version
//      Label       App version
//      Label       App naem
//      Label       Git Commit
//      Label       MAC Address
//      Label       IP Address
//      Label       MQTT Address
//      Label       WIFI SSID
//      Label       Active bit field
//
// 4 Screen Stats
//      Label       Uptime
//      Label       WIFI Num OK/Fail
//      Label       MQTT Num OK/Fail
//      Label       WIFI Status
//      Label       MQTT Status configured/connected
//
// 5 Screen OTA (or as a window?)
//      Button      request OTA
//      Button      reboot
//      Bar         Download in %
//
// 6 Bond devices (or as a window?)
//      List of bond devices
//      Button      delete bond
//
// 7 CSV Download
//      List of devices with status (similar toweb interface)
//      Button      request download
//      Button      store download on SD card
//

lv_obj_t * scr = lv_<type>_create(NULL, copy).



void lv_testdisplay_create()
{
    char buffer[128];

    lv_obj_t * scr = lv_disp_get_scr_act(NULL);     /*Get the current screen*/
    lv_coord_t hres = lv_disp_get_hor_res(NULL);
    lv_coord_t vres = lv_disp_get_ver_res(NULL);

    /*Create a Label on the currently active screen*/
    lv_obj_t * label1 =  lv_label_create(scr, NULL);
    lv_obj_t * label2 =  lv_label_create(scr, NULL);

    /*Modify the Label's text*/
    snprintf(buffer, 128, "hres = %d", hres);
    lv_label_set_text(label1, buffer);
    snprintf(buffer, 128, "vres = %d", vres);
    lv_label_set_text(label2, buffer);

    /* Align the Label to the center
     * NULL means align on parent (which is the screen now)
     * 0, 0 at the end means an x, y offset after alignment*/
    lv_obj_align(label1, NULL, LV_ALIGN_IN_TOP_LEFT, 0, 0);
    lv_obj_align(label2, NULL, LV_ALIGN_IN_BOTTOM_RIGHT, 0, 0);
}
