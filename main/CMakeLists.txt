
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_driver_gpio
        espressif__led_strip
        esp_adc
        esp_wifi
        esp_event
        nvs_flash
        esp_netif
)