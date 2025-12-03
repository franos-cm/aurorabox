#include "button.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "button";

static gpio_num_t s_btn_pin = GPIO_NUM_NC;
static SemaphoreHandle_t s_btn_sem = nullptr;
static volatile int64_t s_last_press_us = 0;

// ISR must be IRAM_ATTR and very small
static void IRAM_ATTR button_isr_handler(void *arg) {
    int64_t now = esp_timer_get_time();
    // simple debounce: ignore edges within 100 ms
    if (now - s_last_press_us < 100000) {
        return;
    }
    s_last_press_us = now;

    BaseType_t hp_task_woken = pdFALSE;
    if (s_btn_sem) {
        xSemaphoreGiveFromISR(s_btn_sem, &hp_task_woken);
    }
    if (hp_task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t button_init(gpio_num_t pin, bool pull_up) {
    s_btn_pin = pin;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = pull_up ? GPIO_INTR_NEGEDGE : GPIO_INTR_POSEDGE;
    io_conf.pull_up_en = pull_up ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = pull_up ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;

    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if (!s_btn_sem) {
        s_btn_sem = xSemaphoreCreateBinary();
        if (!s_btn_sem) return ESP_ERR_NO_MEM;
    }

    // install ISR service once (0 = default flags)
    static bool isr_service_installed = false;
    if (!isr_service_installed) {
        ESP_ERROR_CHECK(gpio_install_isr_service(0));
        isr_service_installed = true;
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(pin, button_isr_handler, nullptr));

    ESP_LOGI(TAG, "Button on GPIO%d initialized (%s, edge=%s)", pin,
             pull_up ? "pull-up, active-low" : "pull-down, active-high", pull_up ? "falling" : "rising");
    return ESP_OK;
}

SemaphoreHandle_t button_get_semaphore(void) { return s_btn_sem; }