#include "rain.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.hpp"

namespace rain_animation {

using namespace utils;

// Initialize rain animation state
void rain_init(RainState &state, cube::Cube &cube, float density, int fall_speed_ms, float trail_strength) {
    state.W = K_MAX_WIDTH;
    state.H = K_MAX_HEIGHT;
    state.D = cube.total_faces();

    // clamp params a bit
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;
    if (fall_speed_ms < 10) fall_speed_ms = 10;
    if (trail_strength < 0.1f) trail_strength = 0.1f;
    if (trail_strength > 1.0f) trail_strength = 1.0f;

    state.density = density;
    state.fall_speed_ms = fall_speed_ms;
    state.trail_strength = trail_strength;
    state.trail_fp = static_cast<uint8_t>(trail_strength * 255.0f);

    // reset droplets
    for (int i = 0; i < RainState::MAX_DROPLETS; ++i) {
        state.drops[i].active = false;
    }

    ESP_ERROR_CHECK(cube.clear());
    vTaskDelay(pdMS_TO_TICKS(50));
}

// Perform one frame / step of the rain animation
void rain_step(RainState &state, cube::Cube &cube) {
    const int W = state.W;
    const int H = state.H;
    const int D = state.D;
    const uint8_t trail_fp = state.trail_fp;

    // 1) Fade toward black for soft trails
    for (int x = 0; x < W; ++x) {
        for (int y = 0; y < H; ++y) {
            for (int z = 0; z < D; ++z) {
                rgb_t c = cube(x, y, z);
                c.r = (uint8_t)((c.r * trail_fp) / 255);
                c.g = (uint8_t)((c.g * trail_fp) / 255);
                c.b = (uint8_t)((c.b * trail_fp) / 255);
                cube(x, y, z) = c;
            }
        }
    }

    // 2) Move droplets down along -y; deactivate when y < 0
    for (int i = 0; i < RainState::MAX_DROPLETS; ++i) {
        auto &drop = state.drops[i];
        if (!drop.active) continue;

        drop.y -= 1; // fall one step

        if (drop.y < 0) {
            drop.active = false;
            continue;
        }

        if (drop.y >= 0 && drop.y < H && drop.x >= 0 && drop.x < W && drop.z >= 0 && drop.z < D) {
            cube(drop.x, drop.y, drop.z) = drop.color;
        }
    }

    // 3) Spawn new droplets at the top layer y = H-1
    float exact_spawn = state.density * (float)(W * D) * 0.5f;
    int spawn_count = (int)exact_spawn;
    if ((esp_random() % 1000) < (uint32_t)((exact_spawn - spawn_count) * 1000.0f)) {
        spawn_count++;
    }

    for (int k = 0; k < spawn_count; ++k) {
        for (int i = 0; i < RainState::MAX_DROPLETS; ++i) {
            auto &drop = state.drops[i];
            if (!drop.active) {
                drop.active = true;
                drop.x = esp_random() % W;
                drop.y = H - 1;
                drop.z = esp_random() % D;
                drop.color = random_color();

                if (drop.z < D) {
                    cube(drop.x, drop.y, drop.z) = drop.color;
                }
                break;
            }
        }
    }

    ESP_ERROR_CHECK(cube.show());
    vTaskDelay(pdMS_TO_TICKS(state.fall_speed_ms));
}

void LightRainAnim::init(Cube &cube) {
    rain_init(state_, cube,
              0.04f, // density
              60,    // fall speed
              0.55f  // trail strength
    );
}

void LightRainAnim::step(Cube &cube) { rain_step(state_, cube); }

void HeavyRainAnim::init(Cube &cube) {
    rain_init(state_, cube,
              0.9f, // density
              60,   // fall speed
              0.55f // trail strength
    );
}

void HeavyRainAnim::step(Cube &cube) { rain_step(state_, cube); }

} // namespace rain_animation
