#
# This is a project Makefile. It is assumed the directory this Makefile resides in is a
# project subdirectory.
#

PROJECT_NAME := blemqttproxy

EXTRA_CFLAGS += --save-temps

# COMPONENT_ADD_INCLUDEDIRS := components/include .

include $(IDF_PATH)/make/project.mk
