set(srcs "main.c"
         "uart.c"
         "tic.c"
         "ha_entities.c")

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES esp_driver_uart
             mqtt-driver
             ha
)

# Resolve version.h, generated into the build dir by the Makefile `version`
# target (scripts/build_version.py).
target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_BINARY_DIR}/"
)
