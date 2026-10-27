#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_now.h"
#include <string.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "led_strip.h"

#define JOY_X_GPIO   GPIO_NUM_4
#define JOY_Y_GPIO   GPIO_NUM_5
#define JOY_SW_GPIO  GPIO_NUM_6
#define JOY_CENTER 1640
#define JOY_DEADBAND 100

#define TAG "joystick"
static led_strip_handle_t led_strip;

void led_init(void) {
    
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = 8,
        .max_leds = 1, // at least one LED on board
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));

    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

void led_set(uint8_t r, uint8_t g, uint8_t b) {

    if (r != 0 || g != 0 || b != 0) {
            led_strip_set_pixel(led_strip, 0, r, g, b); 
            /* Refresh the strip to send data */
            led_strip_refresh(led_strip);
    } 
    else {
            /* Set all LED off to clear all pixels */
            led_strip_clear(led_strip);
    }
}

static adc_oneshot_unit_handle_t adc_handle;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t sw;
} rc_packet_t;


int read_x(void) {
    
    int raw;
    
    adc_oneshot_read(adc_handle, ADC_CHANNEL_4, &raw);
    return raw;
}

int read_y(void) {
    
    int raw;

    adc_oneshot_read(adc_handle, ADC_CHANNEL_5, &raw);
    return raw;
}

int joy_to_percent(int value) {

    int delta = value - JOY_CENTER;
    
    if (abs(delta) < JOY_DEADBAND)
        return 0;

    delta = (delta * 100) / 1640;
    if (delta > 100)
        delta = 100;
    else if (delta < -100)
        delta = -100;
    
    return delta;
}

int read_sw(void) {

    return gpio_get_level(JOY_SW_GPIO);
}

void adc_init(void) {

    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };

    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };

    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_4, &chan_cfg));
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL_5, &chan_cfg));
}

void button_init(void) {

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << JOY_SW_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

void app_main(void) {

    led_init();
    led_set(10, 0, 0); 
    adc_init();
    button_init();
    vTaskDelay(pdMS_TO_TICKS(700));
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg =WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "ESP-NOW init...");
    ESP_ERROR_CHECK(esp_now_init());
    ESP_LOGI(TAG, "Init done");

    esp_now_peer_info_t peer = {0};

    uint8_t receiver_mac[] = {0x7C, 0x4F, 0xAD, 0xBB, 0xD6, 0x81};

    memcpy(peer.peer_addr, receiver_mac, 6);

    peer.channel = 1;
    peer.encrypt = false;

    ESP_ERROR_CHECK(esp_now_add_peer(&peer));    
    ESP_LOGI(TAG, "peer added");
    led_set(0, 10, 0);
    while(1){

        rc_packet_t pkt;

        pkt.x = joy_to_percent(read_x());;
        pkt.y = joy_to_percent(read_y());;
        pkt.sw = read_sw();

        // esp_err_t err = 
        esp_now_send(receiver_mac, (uint8_t *)&pkt, sizeof(pkt));
        
        // printf("Sent packet: X: %d, Y: %d, SW: %d, err: %d\n", x, y, sw, err);
        // printf("\rX: %d, Y: %d, SW: %d", x, y, sw);
        // fflush(stdout);

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

