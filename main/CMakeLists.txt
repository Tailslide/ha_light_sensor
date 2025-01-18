idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver 
            esp_adc
            esp_wifi 
            nvs_flash 
            mqtt 
            esp_timer
)