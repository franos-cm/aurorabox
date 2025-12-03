#include "animations.hpp"
#include "cube.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace cube;
using namespace animations;

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

    rain_animation(cube,
                   0.04f, // density
                   60,    // fall speed
                   0.55f  // trail speed
    );
}
