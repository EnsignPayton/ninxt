idf_component_register(SRCS "ble.c" "uart.c" "ninxt.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt esp_driver_uart esp_driver_gpio nvs_flash)
