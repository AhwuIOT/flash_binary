idf_component_register(SRCS "hello_world_main.c"
                       INCLUDE_DIRS "."
                       REQUIRES nvs_flash esp_http_client wpa_supplicant esp_wifi)
