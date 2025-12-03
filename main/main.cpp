#include "button.hpp"
#include "cube.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rain.hpp"

using namespace cube;
using namespace rain_animation;

extern "C" void app_main(void) {
    PanelChainConfig chains[] = {
        {.pin = 5, .panels = 4, .first_row_backwards = false},
        {.pin = 14, .panels = 4, .first_row_backwards = false},
    };

    CubeCreateArgs args{.backend = Backend::RMT,
                        .chains = chains,
                        .chain_count = 2,
                        .panels_width = K_MAX_WIDTH,
                        .panels_height = K_MAX_HEIGHT};
    Cube cube(args);

    ESP_ERROR_CHECK(cube.clear());
    vTaskDelay(pdMS_TO_TICKS(1000));

    // --- init button: GPIO0, pull-up, active-low, falling-edge ---
    ESP_ERROR_CHECK(button_init(GPIO_NUM_2, /*pull_up=*/true));
    SemaphoreHandle_t btn_sem = button_get_semaphore();

    // --- animation states ---
    static LightRainAnim light_rain;
    static HeavyRainAnim heavy_rain;

    IAnimation *animations[] = {
        &light_rain, &heavy_rain,
        // later: add &plane_sweep, &plasma, ...
    };
    constexpr int ANIM_COUNT = sizeof(animations) / sizeof(animations[0]);

    // -------- Init animations ---------
    int current_index = 0;
    IAnimation *current = animations[current_index];
    current->init(cube);

    while (true) {
        // 1) Run one frame of the current animation
        current->step(cube);

        // 2) Non-blocking check for button press
        if (xSemaphoreTake(btn_sem, 0) == pdTRUE) {
            current_index = (current_index + 1) % ANIM_COUNT;
            current = animations[current_index];
            current->init(cube);
        }
    }
}
