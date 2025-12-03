
#include "include/utils.hpp"
#include "esp_random.h"

namespace utils {

// Avoid too-dim colors: pick each channel in [32, 255]
rgb_t random_color(void) {
    uint8_t r = (esp_random() % 224) + 32;
    uint8_t g = (esp_random() % 224) + 32;
    uint8_t b = (esp_random() % 224) + 32;
    return rgb_t{r, g, b};
}

} // namespace utils