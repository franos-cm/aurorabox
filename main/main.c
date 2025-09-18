#include "led_strip.h"
#include "cube.h"

#define LED_GPIO   2
#define LED_COUNT  64
#define RMT_HZ     (10*1000*1000)

void app_main(void) {
    led_strip_config_t sc = {
        .strip_gpio_num = LED_GPIO,
        .max_leds = LED_COUNT,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
    };
    led_strip_rmt_config_t rc = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = RMT_HZ,
        .mem_block_symbols = 0,
        .flags = { .with_dma = false },   // C6: no RMT-DMA
    };
    led_strip_handle_t strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&sc, &rc, &strip));

    // Simple HSV rainbow
    for (uint8_t h = 0;; h += 2) {
        for (int i = 0; i < LED_COUNT; ++i) {
            hsv_t hsv = { .h = (uint8_t)(h + (i*255)/LED_COUNT), .s = 255, .v = 64 };
            set_pixel_hsv(strip, i, hsv);
        }
        ESP_ERROR_CHECK(led_strip_refresh(strip));
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
