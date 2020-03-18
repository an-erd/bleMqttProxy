#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := blemqttproxy

EXTRA_CFLAGS += --save-temps

# COMPONENT_ADD_INCLUDEDIRS := components/include .
EXTRA_COMPONENT_DIRS := externals/lv_port_esp32/components externals/lv_port_esp32/components/lvgl_esp32_drivers externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_touch externals/lv_port_esp32/components/lvgl_esp32_drivers/lvgl_tft

include $(IDF_PATH)/make/project.mk
