#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

namespace cube {

// Constants for a standard 8x8x8 cube
#define RMT_HZ (10 * 1000 * 1000)
#define K_MAX_RMT_CHAINS 4
#define K_MAX_SPI_CHAINS 1
#define K_MAX_PANELS 8
#define K_MAX_WIDTH 8
#define K_MAX_HEIGHT 8

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

// Backend selection for the LED strip driver
enum class Backend : uint8_t { RMT = 0, SPI = 1 };

struct PanelChainConfig {
    int pin;                  // GPIO number
    uint16_t panels;          // number of 8x8 panels in the chain
    bool first_row_backwards; // row 0 direction
};

// Create-arguments used to construct the Cube
// For SPI backend, chain_count must be exactly 1.
// For RMT backend, chain_count must be in [1, 4].
struct CubeCreateArgs {
    Backend backend;                // backend implementation
    const PanelChainConfig *chains; // pointer to array of chain configs
    size_t chain_count;             // number of elements in chains[]
    size_t panels_width;
    size_t panels_height;
};

// Lightweight validation helper; returns true if args are self-consistent
inline bool validate(const CubeCreateArgs &args) {
    if (args.chains == nullptr || args.chain_count == 0) return false;
    if (args.backend == Backend::SPI && (args.chain_count == 0 || args.chain_count > K_MAX_SPI_CHAINS)) return false;
    if (args.backend == Backend::RMT && (args.chain_count == 0 || args.chain_count > K_MAX_RMT_CHAINS)) return false;
    for (size_t i = 0; i < args.chain_count; ++i) {
        if (args.chains[i].panels == 0) return false;
    }
    if (args.panels_width <= 0 || args.panels_height <= 0) return false;
    return true;
}

class Cube {
  public:
    // ----------------- Basic ops -----------------
    explicit Cube(const CubeCreateArgs &args);
    ~Cube();
    // No copying
    Cube(const Cube &) = delete;
    Cube &operator=(const Cube &) = delete;
    // Allow moving
    Cube(Cube &&) noexcept;
    Cube &operator=(Cube &&) noexcept;

    // ---- Proxy that intercepts assignments to a pixel ----
    // ---- Pixel proxy so `cube(x,y,z) = rgb` writes RAM + HW (x,y,z order) ----
    struct PixelProxy {
        Cube *c;
        uint32_t x, y, z;

        // read current pixel value
        operator rgb_t() const { return c->buf_[z][y][x]; }

        PixelProxy &operator=(rgb_t v) {
            c->buf_[z][y][x] = v;
            // ignore or handle error:
            esp_err_t err = c->poke(x, y, z, v);
            if (err != ESP_OK) assert(0);
            return *this;
        }

        // allow assignment from another proxy
        PixelProxy &operator=(const PixelProxy &other) { return (*this = static_cast<rgb_t>(other)); }
    };

    // ----------------- Accessors -----------------
    uint32_t total_faces() const { return total_faces_; }
    uint32_t total_leds() const { return total_faces_ * panels_width_ * panels_height_; }
    Backend backend() const { return backend_; }
    const PanelChainConfig *chains() const { return chains_; }
    size_t chain_count() const { return chain_count_; }

    // ----------------- Control APIs -----------------
    PixelProxy operator()(uint32_t x, uint32_t y, uint32_t z) {
        assert(z < total_faces_ && y < panels_height_ && x < panels_width_);
        return PixelProxy{this, x, y, z};
    }
    const rgb_t &operator()(uint32_t x, uint32_t y, uint32_t z) const {
        assert(z < total_faces_ && y < panels_height_ && x < panels_width_);
        return buf_[z][y][x];
    }
    esp_err_t clear();
    esp_err_t show();
    void debug_dump() const;

  private:
    Backend backend_;
    PanelChainConfig
        chains_[K_MAX_RMT_CHAINS > K_MAX_SPI_CHAINS ? K_MAX_RMT_CHAINS : K_MAX_SPI_CHAINS]{}; // max of the two
    size_t chain_count_ = 0;
    uint32_t total_faces_ = 0;
    size_t panels_width_ = 0;
    size_t panels_height_ = 0;
    uint32_t pixels_per_face_ = 0;

    // Backend: up to 4 chains for RMT. Each chain has its own strip handle and
    // max_leds
    void *handles_[K_MAX_RMT_CHAINS] = {nullptr, nullptr, nullptr, nullptr};
    uint32_t chain_leds_[K_MAX_RMT_CHAINS] = {0, 0, 0, 0};
    uint32_t handle_count_ = 0;

    rgb_t buf_[K_MAX_PANELS][K_MAX_HEIGHT][K_MAX_WIDTH]{};
    bool chain_dirty_[K_MAX_RMT_CHAINS] = {false, false, false, false}; // Track which chains need refresh

    bool inline map_face_to_chain(uint32_t z, size_t &chain_idx, uint32_t &faces_before) const;
    uint32_t serpentine_index(uint32_t x, uint32_t y, bool first_row_backwards);
    esp_err_t poke(uint32_t x, uint32_t y, uint32_t z, rgb_t v);
};

} // namespace cube
