set(SOURCES
    axp192.c
    beacon.c
    ble_mqtt.c
    ble_myclient.c
    ble_myserver.c
    ble.c
    blemqttproxy.c
    buttons.c
    display.c
    gui.c
    helperfunctions.c
    offlinebuffer.c
    ota.c
    params.c
    ble_sntp.c
    splash.c
    stats.c
    timer.c
    watchdog.c
    web_file_server.c
    wifi.c
    lv_font_montserrat_9.c
    lv_font_montserrat_10.c
    lv_font_montserrat_11.c
    m5stack_16_font_symbol.c
    m5stack_22_font_symbol.c
    m5stack_36_font_symbol.c
    m5stack_48_font_symbol.c
    oled_9_font_symbol.c
    oled_12_font_symbol.c
    oled_16_font_symbol.c
    splash_160x80.c
    splash_oled.c
)

idf_component_register(SRCS ${SOURCES}
    INCLUDE_DIRS .
    REQUIRES lvgl_esp32_drivers lvgl lvgl_tft lvgl_touch nvs_flash bt app_update button esp_http_server esp_http_client mqtt param button
    EMBED_TXTFILES ${project_dir}/server_certs/ca_cert.pem)

# register_component()
