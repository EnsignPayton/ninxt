idf_component_register(SRCS "jb_ble.c" "jb_uart.c" "ninxt.c"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES bt esp_driver_uart nvs_flash)
