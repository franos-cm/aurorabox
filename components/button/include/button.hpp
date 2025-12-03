#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "soc/gpio_num.h"

#ifdef __cplusplus
extern "C" {
#endif

// Configure a simple debounced button on `pin`.
// If `pull_up` is true, config is input + internal pull-up and falling-edge interrupt.
// If false, input + internal pull-down and rising-edge interrupt.
esp_err_t button_init(gpio_num_t pin, bool pull_up);

// Semaphore given once per (debounced) button press.
SemaphoreHandle_t button_get_semaphore(void);

#ifdef __cplusplus
}
#endif