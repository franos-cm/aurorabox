#pragma once

#include "common.hpp"

namespace countdown_animation {

using namespace anim_common;

class CountdownAnim : public IAnimation {
  public:
    void init(Cube &cube) override;
    void step(Cube &cube) override;

  private:
    enum class Phase : uint8_t { DigitFly, Explosion };

    static constexpr uint32_t MAX_TRAIL_LEN = 6;

    int current_digit_ = 9;
    uint32_t frames_left_ = 0; // kept for future tweaks
    Phase phase_ = Phase::DigitFly;
    uint32_t z_pos_ = 0;
    uint32_t explosion_step_ = 0;

    // Z positions for trailing layers; index 0 = newest (brightest), up to MAX_TRAIL_LEN-1 = oldest (dimmest)
    int32_t trail_z_[MAX_TRAIL_LEN] = {-1, -1, -1, -1, -1, -1};
};

} // namespace countdown_animation
