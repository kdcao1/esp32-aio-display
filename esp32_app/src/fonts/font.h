#pragma once

#include <stdint.h>

struct Glyph {
    uint16_t code;
    uint16_t w, h;
    int16_t xo, yo;
    uint16_t advance;
    uint32_t offset;
};

struct Font {
    const uint8_t *bitmap;
    const Glyph *glyphs;
    uint16_t count;
    int16_t ascent;
    int16_t capHeight;
};
