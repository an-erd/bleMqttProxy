
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "lvgl.h"
#include "lv_testdisplay.h"

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
