#include "countdown.hpp"
#include "cube.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.hpp"
#include <math.h>

namespace countdown_animation {

using namespace utils;

// 8x8 bitmaps for digits 0–9 (MSB = x=0, LSB = x=7)
static const uint8_t DIGIT_FONT[10][8] = {
    // 0
    {0b00111100, 0b01100110, 0b01101110, 0b01110110, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    // 1
    {0b00011000, 0b00111000, 0b00011000, 0b00011000, 0b00011000, 0b00011000, 0b00111100, 0b00000000},
    // 2
    {0b00111100, 0b01100110, 0b00000110, 0b00001100, 0b00110000, 0b01100000, 0b01111110, 0b00000000},
    // 3
    {0b00111100, 0b01100110, 0b00000110, 0b00011100, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
    // 4
    {0b00001100, 0b00011100, 0b00101100, 0b01001100, 0b01111110, 0b00001100, 0b00001100, 0b00000000},
    // 5
    {0b01111110, 0b01100000, 0b01111100, 0b00000110, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
    // 6
    {0b00111100, 0b01100110, 0b01100000, 0b01111100, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    // 7
    {0b01111110, 0b00000110, 0b00001100, 0b00011000, 0b00110000, 0b00110000, 0b00110000, 0b00000000},
    // 8
    {0b00111100, 0b01100110, 0b01100110, 0b00111100, 0b01100110, 0b01100110, 0b00111100, 0b00000000},
    // 9
    {0b00111100, 0b01100110, 0b01100110, 0b00111110, 0b00000110, 0b01100110, 0b00111100, 0b00000000},
};

static void draw_digit_on_face(cube::Cube &cube, int digit, uint32_t face, rgb_t base_color, float brightness_factor) {
    if (digit < 0 || digit > 9) return;
    if (face >= cube.total_faces()) return;
    if (brightness_factor < 0.0f) brightness_factor = 0.0f;
    if (brightness_factor > 1.0f) brightness_factor = 1.0f;

    uint8_t r = static_cast<uint8_t>(base_color.r * brightness_factor);
    uint8_t g = static_cast<uint8_t>(base_color.g * brightness_factor);
    uint8_t b = static_cast<uint8_t>(base_color.b * brightness_factor);
    rgb_t color{r, g, b};

    for (uint32_t y = 0; y < K_MAX_HEIGHT; ++y) {
        // flip Y so font row 0 is at the bottom of the cube
        uint32_t fy = K_MAX_HEIGHT - 1 - y;
        uint8_t row = DIGIT_FONT[digit][fy];
        for (uint32_t x = 0; x < K_MAX_WIDTH; ++x) {
            bool on = (row & (1u << (7 - x))) != 0;
            cube(x, y, face) = on ? color : rgb_t{0, 0, 0};
        }
    }
}

// Simple radial explosion from cube center.
// radius: 0..max_radius, color fades over time via multiplier (0..255).
static void draw_explosion_frame(cube::Cube &cube, float radius, uint8_t brightness, float t01) {
    const float cx = (K_MAX_WIDTH - 1) * 0.5f;
    const float cy = (K_MAX_HEIGHT - 1) * 0.5f;
    const float cz = (cube.total_faces() - 1) * 0.5f;
    const float r2 = radius * radius;
    const float band = 1.0f; // thickness of the main explosion shell

    // Color over time: start bluish, go through green, end more reddish.
    float phase = t01;
    uint8_t base_r = (uint8_t)(255.0f * phase);
    uint8_t base_g = (uint8_t)(255.0f * (1.0f - fabsf(phase - 0.5f) * 2.0f)); // peak green at mid
    uint8_t base_b = (uint8_t)(255.0f * (1.0f - phase));

    // 1) Main symmetric shell
    for (uint32_t z = 0; z < cube.total_faces(); ++z) {
        for (uint32_t y = 0; y < K_MAX_HEIGHT; ++y) {
            for (uint32_t x = 0; x < K_MAX_WIDTH; ++x) {
                float dx = (float)x - cx;
                float dy = (float)y - cy;
                float dz = (float)z - cz;
                float d2 = dx * dx + dy * dy + dz * dz;

                bool in_shell = (d2 >= (r2 - band) && d2 <= (r2 + band));
                if (in_shell) {
                    uint8_t r = (uint8_t)((base_r * brightness) / 255u);
                    uint8_t g = (uint8_t)((base_g * brightness) / 255u);
                    uint8_t b = (uint8_t)((base_b * brightness) / 255u);
                    cube(x, y, z) = rgb_t{r, g, b};
                } else {
                    cube(x, y, z) = rgb_t{0, 0, 0};
                }
            }
        }
    }

    // 2) Stronger spark clusters around the shell, more obvious than before
    const int spark_count = 30;           // more sparks
    const float spark_band = band * 2.5f; // allow a bit more spread

    for (int i = 0; i < spark_count; ++i) {
        uint32_t x = esp_random() % K_MAX_WIDTH;
        uint32_t y = esp_random() % K_MAX_HEIGHT;
        uint32_t z = esp_random() % cube.total_faces();

        float dx = (float)x - cx;
        float dy = (float)y - cy;
        float dz = (float)z - cz;
        float d2 = dx * dx + dy * dy + dz * dz;

        // Only keep sparks reasonably near the target radius
        if (d2 < (r2 - spark_band) || d2 > (r2 + spark_band)) continue;

        // Much brighter/whiter than the shell so it clearly pops
        uint8_t r = (uint8_t)std::min<int>(base_r + 120, 255);
        uint8_t g = (uint8_t)std::min<int>(base_g + 120, 255);
        uint8_t b = (uint8_t)std::min<int>(base_b + 120, 255);

        r = (uint8_t)((r * brightness) / 255u);
        g = (uint8_t)((g * brightness) / 255u);
        b = (uint8_t)((b * brightness) / 255u);

        cube(x, y, z) = rgb_t{r, g, b};
    }

    // 3) A few coarse "rays" shooting outwards (fireworks-like streaks)
    const int ray_count = 6;
    for (int i = 0; i < ray_count; ++i) {
        // Random direction vector from center, normalized-ish
        int sx = (int)(esp_random() % K_MAX_WIDTH);
        int sy = (int)(esp_random() % K_MAX_HEIGHT);
        int sz = (int)(esp_random() % cube.total_faces());

        float dx = (float)sx - cx;
        float dy = (float)sy - cy;
        float dz = (float)sz - cz;
        float len = sqrtf(dx * dx + dy * dy + dz * dz);
        if (len < 0.001f) continue;
        dx /= len;
        dy /= len;
        dz /= len;

        // Ray length proportional to current radius, but short
        float ray_len = radius * 0.7f;
        int steps = (int)ray_len;

        for (int s = 0; s < steps; ++s) {
            float t = (float)s / (float)steps;
            int rx = (int)roundf(cx + dx * (radius * t));
            int ry = (int)roundf(cy + dy * (radius * t));
            int rz = (int)roundf(cz + dz * (radius * t));

            if (rx < 0 || ry < 0 || rz < 0) continue;
            if (rx >= (int)K_MAX_WIDTH || ry >= (int)K_MAX_HEIGHT || rz >= (int)cube.total_faces()) continue;

            // Fade along the ray and with overall brightness
            float ray_brightness = (1.0f - t) * (brightness / 255.0f);
            uint8_t r = (uint8_t)(std::min<int>(base_r + 80, 255) * ray_brightness);
            uint8_t g = (uint8_t)(std::min<int>(base_g + 80, 255) * ray_brightness);
            uint8_t b = (uint8_t)(std::min<int>(base_b + 80, 255) * ray_brightness);

            cube((uint32_t)rx, (uint32_t)ry, (uint32_t)rz) = rgb_t{r, g, b};
        }
    }
}

void CountdownAnim::init(Cube &cube) {
    current_digit_ = 9;
    frames_left_ = 0;
    phase_ = Phase::DigitFly;
    z_pos_ = 0;
    explosion_step_ = 0;
    // clear trail history
    for (uint32_t i = 0; i < MAX_TRAIL_LEN; ++i) {
        trail_z_[i] = -1;
    }
    ESP_ERROR_CHECK(cube.clear());
}

void CountdownAnim::step(Cube &cube) {
    switch (phase_) {
    case Phase::DigitFly: {
        // Limit effective trail to at most 2 layers (0 = current, 1 = previous)
        constexpr uint32_t EFFECTIVE_TRAIL_LEN = 2;

        // 1) Soft fade only on trail layers, and hard-clear others to remove old digits.
        const uint8_t fade_fp = 220; // 0..255
        const uint32_t faces = cube.total_faces();

        // Compute the set of z layers that will hold the trail this frame:
        // z_pos_ (current) and optionally z_pos_-1 (previous).
        bool is_trail_layer[K_MAX_PANELS] = {false};
        if (z_pos_ < faces) {
            is_trail_layer[z_pos_] = true;
        }
        if (z_pos_ > 0 && z_pos_ - 1 < faces) {
            is_trail_layer[z_pos_ - 1] = true;
        }

        for (uint32_t z = 0; z < faces; ++z) {
            for (uint32_t y = 0; y < K_MAX_HEIGHT; ++y) {
                for (uint32_t x = 0; x < K_MAX_WIDTH; ++x) {
                    if (is_trail_layer[z]) {
                        // fade on trail layers
                        rgb_t c = cube(x, y, z);
                        c.r = static_cast<uint8_t>((c.r * fade_fp) / 255);
                        c.g = static_cast<uint8_t>((c.g * fade_fp) / 255);
                        c.b = static_cast<uint8_t>((c.b * fade_fp) / 255);
                        cube(x, y, z) = c;
                    } else {
                        // hard clear on non-trail layers to avoid old digits overlapping
                        cube(x, y, z) = rgb_t{0, 0, 0};
                    }
                }
            }
        }

        // 2) Update full trail history (still tracked for smoothness if you want later),
        //    but we will only render the first 2 entries.
        for (uint32_t i = MAX_TRAIL_LEN - 1; i > 0; --i) {
            trail_z_[i] = trail_z_[i - 1];
        }
        trail_z_[0] = static_cast<int32_t>(z_pos_);

        // Base digit color (blue)
        const rgb_t base_color{0, 0, 255};

        // 3) Render only the first EFFECTIVE_TRAIL_LEN entries to avoid long overlap.
        for (uint32_t i = 0; i < EFFECTIVE_TRAIL_LEN; ++i) {
            int32_t z = trail_z_[i];
            if (z < 0) continue;
            if (static_cast<uint32_t>(z) >= faces) continue;

            // Brightness: current layer full, previous dimmer
            float brightness = (i == 0) ? 1.0f : 0.5f;
            draw_digit_on_face(cube, current_digit_, static_cast<uint32_t>(z), base_color, brightness);
        }

        ESP_ERROR_CHECK(cube.show());

        // 4) Advance along Z until we hit the last face
        if (z_pos_ + 1 < cube.total_faces()) {
            ++z_pos_;
        } else {
            // Finished this digit's fly-through; prep next digit
            if (current_digit_ == 0) {
                // After 0, go into explosion
                phase_ = Phase::Explosion;
                explosion_step_ = 0;
            } else {
                --current_digit_;
                z_pos_ = 0;
                // reset trail so new digit starts fresh
                for (uint32_t i = 0; i < MAX_TRAIL_LEN; ++i) {
                    trail_z_[i] = -1;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(80));
        break;
    }

    case Phase::Explosion: {
        // Only the spherical explosion now (no full-cube white flash)
        const int total_steps = 20;
        float t = (float)explosion_step_ / (float)total_steps;
        float max_radius = sqrtf((K_MAX_WIDTH - 1) * (K_MAX_WIDTH - 1) + (K_MAX_HEIGHT - 1) * (K_MAX_HEIGHT - 1) +
                                 (cube.total_faces() - 1) * (cube.total_faces() - 1)) /
                           2.0f;
        float radius = t * max_radius;
        uint8_t brightness = (uint8_t)((1.0f - t) * 255.0f);

        draw_explosion_frame(cube, radius, brightness, t);
        ESP_ERROR_CHECK(cube.show());

        if (explosion_step_ >= (uint32_t)total_steps) {
            // End of explosion: restart countdown from 9
            phase_ = Phase::DigitFly;
            current_digit_ = 9;
            z_pos_ = 0;
            frames_left_ = 0;
            for (uint32_t i = 0; i < MAX_TRAIL_LEN; ++i) {
                trail_z_[i] = -1;
            }
        } else {
            ++explosion_step_;
        }

        vTaskDelay(pdMS_TO_TICKS(60));
        break;
    }
    }
}

} // namespace countdown_animation
