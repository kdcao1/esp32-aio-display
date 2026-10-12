#pragma once

#include <stdint.h>
#include "fonts/font.h"

static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

static inline uint16_t rgb565(uint32_t rgb) {
    return rgb565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

enum Align { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT };

struct ArcStyle {
    float cx, cy;
    float rOuter, thickness;
    float start, sweep;
    int dir;
    uint32_t c0, c1;
    uint8_t alpha;
    float feather;
    bool roundCaps;
};

class Canvas {
public:
    Canvas(uint16_t *pixels, int width, int height) : px(pixels), w(width), h(height) {}

    uint16_t *pixels() { return px; }
    int width() const { return w; }
    int height() const { return h; }

    void clear(uint16_t color);
    void blend(int x, int y, uint32_t rgb, uint8_t alpha);
    void hline(int x0, int x1, int y, uint32_t rgb, uint8_t alpha);
    void vline(int x, int y0, int y1, uint32_t rgb, uint8_t alpha);
    void arc(const ArcStyle &s, float fill);
    void fillCircle(float cx, float cy, float r, uint32_t rgb, uint8_t alpha);

    int textWidth(const Font &font, const char *text) const;
    void text(const Font &font, const char *text, int x, int baseline, uint32_t rgb, Align align = ALIGN_LEFT);

private:
    uint16_t *px;
    int w, h;
};

uint32_t lerpColor(uint32_t a, uint32_t b, float t);
