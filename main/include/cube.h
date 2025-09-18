#pragma once
#include "color.h"
#include "led_strip.h"

static inline void set_pixel_rgb(led_strip_handle_t strip, int i, rgb_t c)
{
    (void)led_strip_set_pixel(strip, i, c.r, c.g, c.b);
}

static inline void set_pixel_hsv(led_strip_handle_t strip, int i, hsv_t hsv)
{
    rgb_t c = hsv2rgb_raw(hsv);
    (void)led_strip_set_pixel(strip, i, c.r, c.g, c.b);
}

static inline void fill_rgb(led_strip_handle_t strip, int n, rgb_t c)
{
    for (int i = 0; i < n; ++i)
        set_pixel_rgb(strip, i, c);
}
static inline void fill_hsv(led_strip_handle_t strip, int n, hsv_t hsv)
{
    for (int i = 0; i < n; ++i)
        set_pixel_hsv(strip, i, hsv);
}