#include "animations.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "utils.hpp"

namespace animations {

using namespace utils;

struct Droplet {
    int x, y, z; // y: height (0..7), x,z: horizontal coordinates
    rgb_t color;
    bool active;
};

// density ∈ [0,1], higher = more drops
// fall_speed_ms: time between steps
// trail_strength ∈ (0,1], closer to 1 = slower fade
void rain_animation(cube::Cube &cube, float density = 0.5f, int fall_speed_ms = 70, float trail_strength = 0.7f) {
    const int W = K_MAX_WIDTH;        // x: 0..7
    const int H = K_MAX_HEIGHT;       // y: 0..7 (vertical)
    const int D = cube.total_faces(); // z: 0..D-1 faces

    // clamp params a bit
    if (density < 0.0f) density = 0.0f;
    if (density > 1.0f) density = 1.0f;
    if (fall_speed_ms < 10) fall_speed_ms = 10;
    if (trail_strength < 0.1f) trail_strength = 0.1f;
    if (trail_strength > 1.0f) trail_strength = 1.0f;

    // convert trail_strength to fixed-point factor in [0,255]
    const uint8_t trail_fp = static_cast<uint8_t>(trail_strength * 255.0f);

    static constexpr int MAX_DROPLETS = 256;
    // Use static to avoid stack overflow with large array
    static Droplet drops[MAX_DROPLETS];

    for (int i = 0; i < MAX_DROPLETS; ++i)
        drops[i].active = false;

    ESP_ERROR_CHECK(cube.clear());
    vTaskDelay(pdMS_TO_TICKS(50));

    while (true) {
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
        for (int i = 0; i < MAX_DROPLETS; ++i) {
            if (!drops[i].active) continue;

            drops[i].y -= 1; // fall one step

            if (drops[i].y < 0) {
                drops[i].active = false;
                continue;
            }

            // Draw new position
            if (drops[i].y >= 0 && drops[i].y < H && drops[i].x >= 0 && drops[i].x < W && drops[i].z >= 0 &&
                drops[i].z < D) {
                cube(drops[i].x, drops[i].y, drops[i].z) = drops[i].color;
            }
        }

        // 3) Spawn new droplets at the top layer y = H-1
        // Calculate number of drops to spawn this frame based on density
        // density=1.0 -> spawn roughly W*D/2 drops per frame (heavy rain)
        float exact_spawn = density * (float)(W * D) * 0.5f;
        int spawn_count = (int)exact_spawn;
        // Probabilistic adjustment for fractional part
        if ((esp_random() % 1000) < (uint32_t)((exact_spawn - spawn_count) * 1000.0f)) {
            spawn_count++;
        }

        for (int k = 0; k < spawn_count; ++k) {
            // Try to find a free slot
            for (int i = 0; i < MAX_DROPLETS; ++i) {
                if (!drops[i].active) {
                    drops[i].active = true;
                    drops[i].x = esp_random() % W;
                    drops[i].y = H - 1; // spawn at top
                    drops[i].z = esp_random() % D;
                    drops[i].color = random_color();

                    // Draw immediately
                    if (drops[i].z < D) { // Safety check
                        cube(drops[i].x, drops[i].y, drops[i].z) = drops[i].color;
                    }
                    break;
                }
            }
        }

        ESP_ERROR_CHECK(cube.show());
        vTaskDelay(pdMS_TO_TICKS(fall_speed_ms));
    }
}

} // namespace animations
