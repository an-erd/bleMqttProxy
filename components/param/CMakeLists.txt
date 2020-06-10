set(SOURCES param.c)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ./include
                    REQUIRES spi_flash nvs_flash)
