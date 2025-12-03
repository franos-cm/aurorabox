#pragma once

#include "cube.hpp"
#include <stddef.h>
#include <stdint.h>

namespace animations {
void rain_animation(cube::Cube &cube, float density, int fall_speed_ms, float trail_strength);
}
