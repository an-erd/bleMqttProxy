#ifndef __BUTTONS_H__
#define __BUTTONS_H__

/* IOT-Buttons */
#define MAX_BUTTON_COUNT 3
#define BUTTON_ACTIVE_LEVEL 0
// static int8_t btn[MAX_BUTTON_COUNT][2] = { {0, CONFIG_BUTTON_1_PIN}, {1, CONFIG_BUTTON_2_PIN}, {2, CONFIG_BUTTON_3_PIN} };
// static button_handle_t btn_handle[MAX_BUTTON_COUNT];
// static int64_t time_button_long_press[MAX_BUTTON_COUNT] = { 0 };

void initialize_buttons();

#endif // __BUTTONS_H__