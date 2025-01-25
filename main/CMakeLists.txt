idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "include" "." "../traps/${TRAP_ID}"
    REQUIRES driver
            esp_adc
            esp_wifi
            nvs_flash
            mqtt
            esp_timer
            freertos
            esp_event
            esp_common
            esp_hw_support
            led_strip
)

# Copy trap-specific config to build directory
add_custom_command(
    TARGET ${COMPONENT_LIB} PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/../traps/${TRAP_ID}/config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/config.h
    COMMENT "Copying ${TRAP_ID} config.h"
)