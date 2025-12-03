#pragma once

#include "common.hpp"

namespace circle_animation {

using namespace anim_common;

class CircleSpinAnim : public IAnimation {
  public:
    void init(Cube &cube) override;
    void step(Cube &cube) override;

  private:
    uint32_t frame_ = 0;
};

} // namespace circle_animation
