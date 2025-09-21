#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "cube.hpp"

using namespace cube;

extern "C" void app_main(void) {
  // --- Define your panel chains (GPIOs and wiring) ---
  printf("AAAAAAAAA\n");

  PanelChainConfig chains[] = {
      {.pin = 3, .panels = 1, .first_row_backwards = false},
  };

  CubeCreateArgs args{
      .backend = Backend::RMT,
      .chains = chains,
      .chain_count = 1,
  };

  Cube cube(args);

  printf("Before loop\n");

  for (uint32_t y = 0; y < kPanelHeight; ++y)
    for (uint32_t x = 0; x < kPanelWidth; ++x)
      cube(x, y, 0) = rgb_t{0, 0, 0};

  printf("After loop\n");

  ESP_ERROR_CHECK(cube.show());
  vTaskDelay(pdMS_TO_TICKS(4000));

  printf("After loop show\n");


  cube(3, 0, 0) = rgb_t{0, 0, 0};
  printf("After single\n");
  ESP_ERROR_CHECK(cube.show());
  printf("After show\n");
  vTaskDelay(pdMS_TO_TICKS(1000));

  printf("After error \n");

  while (1) {
    vTaskDelay(portMAX_DELAY);
  }

    // // --- Animate a pixel across face 1 (magenta) ---
    // uint32_t z = 0, x = 0, y = 0;
    // int dx = 1, dy = 1;

  
    // while (true) {
    //     // draw pixel
    //     cube(x, y, z) = rgb_t{20, 0, 20};
    //     ESP_ERROR_CHECK(cube.show());

    //     vTaskDelay(pdMS_TO_TICKS(100));

    //     // erase previous pixel
    //     cube(x,y,z) = rgb_t{0, 0, 0};
    //     ESP_ERROR_CHECK(cube.show());

    //     // update position with simple bounce
    //     uint32_t nx = x + dx;
    //     uint32_t ny = y + dy;
    //     if (nx >= kPanelWidth)  { dx = -dx; nx = x + dx; }
    //     if (ny >= kPanelHeight) { dy = -dy; ny = y + dy; }
    //     x = nx; y = ny;
    // }
}
