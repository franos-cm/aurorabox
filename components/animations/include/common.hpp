#pragma once

#include "cube.hpp"
#include <stddef.h>
#include <stdint.h>

namespace anim_common {

using cube::Cube;
using utils::rgb_t;

// ------------------- Core animation interface -------------------

struct IAnimation {
    virtual ~IAnimation() = default;
    virtual void init(Cube &cube) = 0;
    virtual void step(Cube &cube) = 0;
};

// Common base state info (frame counter, etc.)
struct BaseAnimState {
    uint32_t frame = 0;
};

} // namespace anim_common
