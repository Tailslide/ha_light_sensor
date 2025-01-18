#idf_component_register(SRCS "main.c"
#                    PRIV_REQUIRES spi_flash
#                    INCLUDE_DIRS "")
#idf_component_register(SRCS "hello_world_main.c"
#                      INCLUDE_DIRS "."
#                      REQUIRES esp_driver_uart)
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_adc
)