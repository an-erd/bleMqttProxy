set(COMPONENT_SRCS "blemqttproxy.c")
set(COMPONENT_ADD_INCLUDEDIRS ".")

set(REQUIRES lvgl_tft lvgl_touch lvgl)

# Embed the server root certificate into the final binary
set(COMPONENT_EMBED_TXTFILES ${IDF_PROJECT_PATH}/server_certs/ca_cert.pem)

register_component()
