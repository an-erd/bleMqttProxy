# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# set(EXTRA_COMPONENT_DIRS externals/lv_port_esp32/components)

set(EXTRA_COMPONENT_DIRS externals/lv_port_esp32/components externals/lv_port_esp32/components/lvgl_esp32_drivers externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_touch externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_tft)

project(blemqttproxy)
