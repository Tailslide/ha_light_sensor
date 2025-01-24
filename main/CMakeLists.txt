idf_component_register(
    SRC_DIRS "src"
    INCLUDE_DIRS "include" "."
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