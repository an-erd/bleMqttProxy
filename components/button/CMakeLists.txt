set(SOURCES button.c)
idf_component_register(SRCS ${SOURCES}
                    INCLUDE_DIRS ./include
                    )
