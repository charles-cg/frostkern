idf_component_register(SRCS "frostkern_main.c" "app_uart_protocol.c"
                    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_timer
                                  esp_driver_uart esp_http_client mbedtls
                    INCLUDE_DIRS ".")
