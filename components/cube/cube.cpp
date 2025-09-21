// Cube implementation with RMT backend only for now
#include "include/cube.hpp"
#include "led_strip.h"
#include <assert.h>
#include <string.h>

namespace cube {

Cube::Cube(const CubeCreateArgs &args)
    : backend_(args.backend), chain_count_(args.chain_count), panels_width_(args.panels_width),
      panels_height_(args.panels_height) {
    assert(validate(args) && "invalid CubeCreateArgs");
    uint32_t faces = 0;
    for (size_t i = 0; i < chain_count_; ++i) {
        chains_[i] = args.chains[i];
        faces += chains_[i].panels;
    }
    total_faces_ = faces;
    pixels_per_face_ = panels_width_ * panels_height_;

    // Initialize backend (RMT only)
    if (backend_ == Backend::RMT) {
        handle_count_ = chain_count_;
        uint32_t faces_base = 0;
        for (size_t i = 0; i < chain_count_; ++i) {
            const auto &ch = chains_[i];
            const uint32_t leds = ch.panels * panels_width_ * panels_height_;
            chain_leds_[i] = leds;

            led_strip_config_t sc = {};
            sc.strip_gpio_num = ch.pin;
            sc.max_leds = leds;
            sc.led_model = LED_MODEL_WS2812;
            sc.color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
            sc.flags.invert_out = false;

            led_strip_rmt_config_t rc = {};
            rc.clk_src = RMT_CLK_SRC_DEFAULT;
            rc.resolution_hz = RMT_HZ;
            rc.mem_block_symbols = 0; // default
            rc.flags.with_dma = 0;    // C6: no RMT-DMA

            led_strip_handle_t handle;
            esp_err_t err = led_strip_new_rmt_device(&sc, &rc, &handle);
            assert(err == ESP_OK && "failed to create RMT led strip");
            handles_[i] = handle;
            (void)faces_base; // reserved if we later track per-chain face offsets
            faces_base += ch.panels;
        }
    }
}

Cube::Cube(Cube &&other) noexcept
    : backend_(other.backend_), chain_count_(other.chain_count_), total_faces_(other.total_faces_) {
    for (size_t i = 0; i < other.chain_count_; ++i)
        chains_[i] = other.chains_[i];
    other.chain_count_ = 0;
    other.total_faces_ = 0;
}

Cube &Cube::operator=(Cube &&other) noexcept {
    if (this != &other) {
        backend_ = other.backend_;
        chain_count_ = other.chain_count_;
        for (size_t i = 0; i < other.chain_count_; ++i)
            chains_[i] = other.chains_[i];
        total_faces_ = other.total_faces_;
        other.chain_count_ = 0;
        other.total_faces_ = 0;
    }
    return *this;
}

Cube::~Cube() {
    if (backend_ == Backend::RMT) {
        for (size_t i = 0; i < handle_count_; ++i) {
            if (handles_[i]) {
                led_strip_del((led_strip_handle_t)handles_[i]);
                handles_[i] = nullptr;
            }
        }
    }
}

bool Cube::map_face_to_chain(uint32_t z, size_t &chain_idx, uint32_t &faces_before) const {
    if (z >= total_faces_) return false;
    uint32_t acc = 0;
    for (size_t i = 0; i < chain_count_; ++i) {
        const uint32_t next = acc + chains_[i].panels;
        if (z < next) {
            chain_idx = i;
            faces_before = acc;
            return true;
        }
        acc = next;
    }
    return false;
}

inline uint32_t Cube::serpentine_index(uint32_t x, uint32_t y, bool first_row_backwards) {
    const bool row_back = (y % 2 == 0) ? first_row_backwards : !first_row_backwards;
    const uint32_t col = row_back ? (panels_width_ - 1 - x) : x;
    return y * panels_width_ + col;
}

esp_err_t Cube::poke(uint32_t x, uint32_t y, uint32_t z, rgb_t v) {
    if (z >= total_faces_ || y >= panels_height_ || x >= panels_width_) return ESP_ERR_INVALID_ARG;

    size_t ci;
    uint32_t faces_before;
    if (!map_face_to_chain(z, ci, faces_before)) return ESP_ERR_INVALID_STATE;

    auto h = (led_strip_handle_t)handles_[ci];
    if (!h) return ESP_ERR_INVALID_STATE;

    const uint32_t local_face = z - faces_before;
    const uint32_t base = local_face * pixels_per_face_;
    const uint32_t idx = serpentine_index(x, y, chains_[ci].first_row_backwards);
    const uint32_t led = base + idx;

    esp_err_t err = led_strip_set_pixel(h, led, v.r, v.g, v.b);
    if (err != ESP_OK) return err;

    chain_dirty_[ci] = true;
    return ESP_OK;
}

esp_err_t Cube::clear() {
    if (backend_ != Backend::RMT) return ESP_ERR_NOT_SUPPORTED;

    for (size_t ci = 0; ci < handle_count_; ++ci) {
        auto h = (led_strip_handle_t)handles_[ci];
        if (!h) return ESP_ERR_INVALID_STATE;

        esp_err_t err = led_strip_clear(h);
        if (err != ESP_OK) return err;

        chain_dirty_[ci] = true; // mark so show() will refresh
    }
    return show();
}

esp_err_t Cube::show() {
    if (backend_ != Backend::RMT) return ESP_ERR_NOT_SUPPORTED;

    for (size_t ci = 0; ci < handle_count_; ++ci) {
        if (!chain_dirty_[ci]) continue;
        auto h = (led_strip_handle_t)handles_[ci];
        if (!h) return ESP_ERR_INVALID_STATE;
        esp_err_t err = led_strip_refresh(h);
        if (err != ESP_OK) return err;
        chain_dirty_[ci] = false;
    }
    return ESP_OK;
}

void Cube::debug_dump() const {
    printf("\n=== Cube buffer dump ===\n");
    for (uint32_t z = 0; z < total_faces_; ++z) {
        printf("Face %lu:\n", (unsigned long)z);
        for (uint32_t y = 0; y < panels_height_; ++y) {
            printf("  y=%lu: ", (unsigned long)y);
            for (uint32_t x = 0; x < panels_width_; ++x) {
                const rgb_t &c = buf_[z][y][x];
                // print as hex for compactness
                printf("(%02x,%02x,%02x) ", c.r, c.g, c.b);
            }
            printf("\n");
        }
    }
}

} // namespace cube
