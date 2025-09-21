#include "cube.hpp"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

using namespace cube;

static inline bool in_bounds(int x, int y, int W, int H) { return x >= 0 && x < W && y >= 0 && y < H; }

// pick a ±90° perpendicular *offset* to (dx,dy) that stays in-bounds from (x,y)
static inline bool pick_perp_offset(int dx, int dy, int x, int y, int W, int H, int &ox, int &oy) {
    // two perpendicular choices: rotate (dx,dy) by ±90°
    int px1 = -dy, py1 = dx; // left nudge
    int px2 = dy, py2 = -dx; // right nudge
    bool c1 = in_bounds(x + px1, y + py1, W, H);
    bool c2 = in_bounds(x + px2, y + py2, W, H);
    if (c1 && c2) {
        if (esp_random() & 1) {
            ox = px1;
            oy = py1;
        } else {
            ox = px2;
            oy = py2;
        }
        return true;
    } else if (c1) {
        ox = px1;
        oy = py1;
        return true;
    } else if (c2) {
        ox = px2;
        oy = py2;
        return true;
    }
    return false;
}

static inline int rnd_sign() { return (esp_random() & 1) ? 1 : -1; }

void dvd_bounce(Cube &cube) {
    const int W = K_MAX_WIDTH;
    const int H = K_MAX_HEIGHT;

    // start at random cell, with a *diagonal* heading (dx,dy ∈ {±1})
    int x = esp_random() % W;
    int y = esp_random() % H;
    int dx = rnd_sign();
    int dy = rnd_sign();

    // color (change only on bounce)
    uint8_t r = 32, g = 0, b = 0;

    while (true) {
        // draw + show
        cube(x, y, 0) = rgb_t{r, g, b};
        ESP_ERROR_CHECK(cube.show());
        vTaskDelay(pdMS_TO_TICKS(100));

        // erase + show (no trail)
        cube(x, y, 0) = rgb_t{0, 0, 0};
        ESP_ERROR_CHECK(cube.show());

        // --- forward diagonal step (always) ---
        int nx = x + dx;
        int ny = y + dy;

        bool hitX = (nx < 0 || nx >= W);
        bool hitY = (ny < 0 || ny >= H);

        if (hitX || hitY) {
            // pure specular reflection (DVD)
            if (hitX) dx = -dx;
            if (hitY) dy = -dy;

            // change color on every bounce
            if (r > 0 && b == 0) {
                r--;
                g++;
            } // red → green
            else if (g > 0 && r == 0) {
                g--;
                b++;
            } // green → blue
            else if (b > 0 && g == 0) {
                b--;
                r++;
            } // blue → red

            // take the reflected diagonal step
            nx = x + dx;
            ny = y + dy;

            // corner “dislodge”: add one perpendicular nudge so we don’t ping-pong the same
            // diagonal
            if (hitX && hitY) {
                int ox = 0, oy = 0;
                if (pick_perp_offset(dx, dy, nx, ny, W, H, ox, oy)) {
                    nx += ox;
                    ny += oy; // nudge AFTER the reflected step
                }
            } else {
                // optional: small chance to nudge after any wall bounce too
                if ((esp_random() % 4) == 0) { // 25% on bounces; tune or remove
                    int ox = 0, oy = 0;
                    if (pick_perp_offset(dx, dy, nx, ny, W, H, ox, oy)) {
                        nx += ox;
                        ny += oy;
                    }
                }
            }

            // commit
            x = nx;
            y = ny;
            continue;
        }

        // free flight: occasionally add a perpendicular nudge *in addition* to the forward move
        // (keeps heading; avoids the same mirrored loop forever)
        int ox = 0, oy = 0;
        if ((esp_random() % 12) == 0 && pick_perp_offset(dx, dy, nx, ny, W, H, ox, oy)) {
            nx += ox;
            ny += oy; // add side-step; heading dx,dy unchanged
        }

        x = nx;
        y = ny;
    }
}

extern "C" void app_main(void) {
    PanelChainConfig chains[] = {
        {.pin = 3, .panels = 1, .first_row_backwards = true},
    };

    CubeCreateArgs args{.backend = Backend::RMT,
                        .chains = chains,
                        .chain_count = 1,
                        .panels_width = K_MAX_WIDTH,
                        .panels_height = K_MAX_HEIGHT};

    Cube cube(args);

    ESP_ERROR_CHECK(cube.clear());
    vTaskDelay(pdMS_TO_TICKS(1000));

    dvd_bounce(cube);
}
