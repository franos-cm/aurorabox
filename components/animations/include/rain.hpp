#pragma once

#include "common.hpp"
#include <stddef.h>
#include <stdint.h>

namespace rain_animation {

using namespace anim_common;

// ------------------- Rain primitive state & functions -------------------
struct RainState : public BaseAnimState {
    struct Droplet {
        int x, y, z;
        rgb_t color;
        bool active;
    };

    static constexpr int MAX_DROPLETS = 256;

    Droplet drops[MAX_DROPLETS];
    int W; // width
    int H; // height
    int D; // depth (faces)
    float density;
    int fall_speed_ms;
    float trail_strength;
    uint8_t trail_fp; // fixed point fade factor [0,255]
};

// Low-level, reusable building blocks
void rain_init(RainState &state, Cube &cube, float density, int fall_speed_ms, float trail_strength);
void rain_step(RainState &state, Cube &cube);

// ------------------- High-level animations -------------------

class LightRainAnim : public IAnimation {
  public:
    void init(Cube &cube) override;
    void step(Cube &cube) override;

  private:
    RainState state_{};
};

class HeavyRainAnim : public IAnimation {
  public:
    void init(Cube &cube) override;
    void step(Cube &cube) override;

  private:
    RainState state_{};
};

} // namespace rain_animation