#include "circle.hpp"
#include "cube.hpp"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.hpp"
#include <math.h>

namespace circle_animation {

using namespace utils;
using namespace cube;

// Simple HSV→RGB helper (H in [0,1), S,V in [0,1])
static rgb_t hsv_to_rgb(float h, float s, float v) {
    if (s <= 0.0f) {
        uint8_t c = static_cast<uint8_t>(v * 255.0f);
        return rgb_t{c, c, c};
    }
    h = fmodf(fmaxf(h, 0.0f), 1.0f) * 6.0f;
    int i = static_cast<int>(h);
    float f = h - static_cast<float>(i);
    float p = v * (1.0f - s);
    float q = v * (1.0f - s * f);
    float t = v * (1.0f - s * (1.0f - f));

    float r, g, b;
    switch (i) {
    default:
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
    }
    return rgb_t{static_cast<uint8_t>(r * 255.0f), static_cast<uint8_t>(g * 255.0f), static_cast<uint8_t>(b * 255.0f)};
}

// Draw a filled circle in the XY plane, spinning around Y so it sweeps along Z.
void CircleSpinAnim::init(Cube &cube) {
    frame_ = 0;
    ESP_ERROR_CHECK(cube.clear());
}

void CircleSpinAnim::step(Cube &cube) {
    const uint32_t faces = cube.total_faces();
    if (faces == 0) {
        vTaskDelay(pdMS_TO_TICKS(50));
        return;
    }

    // 1) Soft global fade toward black (like rain), no hard clears -> no flicker
    const uint8_t fade_fp = 230; // 0..255, closer to 255 => slower fade
    for (uint32_t z = 0; z < faces; ++z) {
        for (uint32_t y = 0; y < K_MAX_HEIGHT; ++y) {
            for (uint32_t x = 0; x < K_MAX_WIDTH; ++x) {
                rgb_t c = cube(x, y, z);
                c.r = static_cast<uint8_t>((c.r * fade_fp) / 255);
                c.g = static_cast<uint8_t>((c.g * fade_fp) / 255);
                c.b = static_cast<uint8_t>((c.b * fade_fp) / 255);
                cube(x, y, z) = c;
            }
        }
    }

    // 2) Geometry: cube center
    const float cx = (K_MAX_WIDTH - 1) * 0.5f;
    const float cy = (K_MAX_HEIGHT - 1) * 0.5f;
    const float cz = (faces - 1) * 0.5f;

    // Circle radius in XY; circumference ≈ 9 => r ≈ 9 / (2π) ≈ 1.43.
    // Use that directly (it will still look nice on the 8×8 grid).
    const float radius = 3.5f;
    const float radius2 = radius * radius;

    // 3) Animation parameters: spin around Y, color cycle
    const float spin_speed = 0.15f; // faster rotation
    const float hue_speed = 0.01f;

    float angle = frame_ * spin_speed;
    float hue = fmodf(frame_ * hue_speed, 1.0f);
    rgb_t color = hsv_to_rgb(hue, 1.0f, 1.0f);

    // Rotation around Y: we rotate the *points* backwards so the disc effectively spins forward.
    float cosA = cosf(-angle);
    float sinA = sinf(-angle);

    // 4) For each voxel, test membership in the spinning disc
    // Base disc: XY plane, centered at (cx,cy), lying at z'=0 in rotated coordinates.
    // Condition: |z'| small and x'^2 + y'^2 <= radius^2.
    const float plane_thickness = 0.6f; // how "thick" the disc is along its normal

    for (uint32_t z = 0; z < faces; ++z) {
        for (uint32_t y = 0; y < K_MAX_HEIGHT; ++y) {
            for (uint32_t x = 0; x < K_MAX_WIDTH; ++x) {
                // Centered coordinates
                float dx = static_cast<float>(x) - cx;
                float dy = static_cast<float>(y) - cy;
                float dz = static_cast<float>(z) - cz;

                // Rotate around Y by -angle
                float xr = dx * cosA + dz * sinA;
                float yr = dy;
                float zr = -dx * sinA + dz * cosA;

                // Check if point lies near the z'=0 plane (the disc plane)
                if (fabsf(zr) > plane_thickness) continue;

                // Check if within circle in XY
                float dist2 = xr * xr + yr * yr;
                if (dist2 > radius2) continue;

                cube(x, y, z) = color;
            }
        }
    }

    ESP_ERROR_CHECK(cube.show());
    ++frame_;
    vTaskDelay(pdMS_TO_TICKS(60));
}

} // namespace circle_animation
