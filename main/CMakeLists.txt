set(SOURCES "blemqttproxy.c" "beacon.c" "ble.c" "ble_mqtt.c" "display.c" "helperfunctions.c" "offlinebuffer.c" "ota.c" "timer.c" "web_file_server.c" "oled_9_font_symbol.c" "oled_12_font_symbol.c" "oled_16_font_symbol.c" "splash_oled.c" "splash_160x80.c")

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS .
    REQUIRES lvgl_esp32_drivers lvgl lv_examples lvgl_tft lvgl_touch nvs_flash bt app_update button esp_http_server esp_http_client mqtt param button
    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)

# register_component()
