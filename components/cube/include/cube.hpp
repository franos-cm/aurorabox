#pragma once
#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

namespace cube {

// Constants for a standard 8x8 panel
#define RMT_HZ (10 * 1000 * 1000);
static constexpr uint8_t kPanelWidth = 8;
static constexpr uint8_t kPanelHeight = 1;
static constexpr uint8_t kMaxRmtChains = 4;
static constexpr uint8_t kMaxSpiChains = 1;
static constexpr uint8_t kMaxFaces = 8;

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

  // RMT-specific defaults
  uint32_t rmt_resolution_hz = 10 * 1000 * 1000; // 10 MHz
  bool invert_out = true;                        // invert signal output
};

// Lightweight validation helper; returns true if args are self-consistent
inline bool validate(const CubeCreateArgs &args) {
  if (args.chains == nullptr || args.chain_count == 0)
    return false;
  if (args.backend == Backend::SPI &&
      (args.chain_count == 0 || args.chain_count > kMaxSpiChains))
    return false;
  if (args.backend == Backend::RMT &&
      (args.chain_count == 0 || args.chain_count > kMaxRmtChains))
    return false;
  for (size_t i = 0; i < args.chain_count; ++i) {
    if (args.chains[i].panels == 0)
      return false;
  }
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
      if (err != ESP_OK)
        assert(0);
      return *this;
    }

    // allow assignment from another proxy
    PixelProxy &operator=(const PixelProxy &other) {
      return (*this = static_cast<rgb_t>(other));
    }
  };

  // ----------------- Accessors -----------------
  uint32_t total_faces() const { return total_faces_; }
  uint32_t total_leds() const {
    return total_faces_ * kPanelWidth * kPanelHeight;
  }
  Backend backend() const { return backend_; }
  const PanelChainConfig *chains() const { return chains_; }
  size_t chain_count() const { return chain_count_; }

  // ----------------- Control APIs -----------------
  PixelProxy operator()(uint32_t x, uint32_t y, uint32_t z) {
    assert(z < total_faces_ && y < kPanelHeight && x < kPanelWidth);
    return PixelProxy{this, x, y, z};
  }
  const rgb_t &operator()(uint32_t x, uint32_t y, uint32_t z) const {
    assert(z < total_faces_ && y < kPanelHeight && x < kPanelWidth);
    return buf_[z][y][x];
  }
  esp_err_t show();

private:
  Backend backend_;
  PanelChainConfig chains_[kMaxRmtChains > kMaxSpiChains
                               ? kMaxRmtChains
                               : kMaxSpiChains]{}; // max of the two
  size_t chain_count_ = 0;
  uint32_t total_faces_ = 0;

  // Backend: up to 4 chains for RMT. Each chain has its own strip handle and
  // max_leds
  void *handles_[kMaxRmtChains] = {nullptr, nullptr, nullptr, nullptr};
  uint32_t chain_leds_[kMaxRmtChains] = {0, 0, 0, 0};
  uint32_t handle_count_ = 0;

  rgb_t buf_[kMaxFaces][kPanelHeight][kPanelWidth]{};
  bool chain_dirty_[kMaxRmtChains] = {false, false, false,
                                      false}; // Track which chains need refresh

  bool inline map_face_to_chain(uint32_t z, size_t &chain_idx,
                                uint32_t &faces_before) const;
  static inline uint32_t serpentine_index(uint32_t x, uint32_t y,
                                          bool first_row_backwards);
  esp_err_t poke(uint32_t x, uint32_t y, uint32_t z, rgb_t v);
};

} // namespace cube
