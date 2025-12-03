#pragma once
#include <stdint.h>

namespace utils {

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

rgb_t random_color(void);

} // namespace utils
