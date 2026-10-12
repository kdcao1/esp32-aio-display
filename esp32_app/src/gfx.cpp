#include "gfx.h"

#include <math.h>
#include <string.h>

static inline float clamp01(float v) {
    return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
}

static inline float wrap360(float a) {
    a = fmodf(a, 360.0f);
    return a < 0.0f ? a + 360.0f : a;
}

uint32_t lerpColor(uint32_t a, uint32_t b, float t) {
    t = clamp01(t);
    int r = ((a >> 16) & 0xFF) + (int)((((int)(b >> 16) & 0xFF) - (int)((a >> 16) & 0xFF)) * t);
    int g = ((a >> 8) & 0xFF) + (int)((((int)(b >> 8) & 0xFF) - (int)((a >> 8) & 0xFF)) * t);
    int bl = (a & 0xFF) + (int)((((int)b & 0xFF) - (int)(a & 0xFF)) * t);
    return (r << 16) | (g << 8) | bl;
}

void Canvas::clear(uint16_t color) {
    if (color == 0) {
        memset(px, 0, (size_t)w * h * 2);
        return;
    }
    for (int i = 0; i < w * h; i++) px[i] = color;
}

void Canvas::blend(int x, int y, uint32_t rgb, uint8_t alpha) {
    if (x < 0 || y < 0 || x >= w || y >= h || alpha == 0) return;
    int tx = x, ty = y;
    switch (rot) {
    case 1: tx = w - 1 - y; ty = x; break;
    case 2: tx = w - 1 - x; ty = h - 1 - y; break;
    case 3: tx = y; ty = h - 1 - x; break;
    }
    uint16_t &d = px[ty * w + tx];
    if (alpha == 255) {
        d = rgb565(rgb);
        return;
    }
    int dr = (d >> 11) << 3, dg = ((d >> 5) & 0x3F) << 2, db = (d & 0x1F) << 3;
    int sr = (rgb >> 16) & 0xFF, sg = (rgb >> 8) & 0xFF, sb = rgb & 0xFF;
    dr += ((sr - dr) * alpha) >> 8;
    dg += ((sg - dg) * alpha) >> 8;
    db += ((sb - db) * alpha) >> 8;
    d = rgb565(dr, dg, db);
}

void Canvas::hline(int x0, int x1, int y, uint32_t rgb, uint8_t alpha) {
    for (int x = x0; x <= x1; x++) blend(x, y, rgb, alpha);
}

void Canvas::vline(int x, int y0, int y1, uint32_t rgb, uint8_t alpha) {
    for (int y = y0; y <= y1; y++) blend(x, y, rgb, alpha);
}

void Canvas::fillCircle(float cx, float cy, float r, uint32_t rgb, uint8_t alpha) {
    int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1);
    int y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float cov = clamp01(r - sqrtf(dx * dx + dy * dy) + 0.5f);
            if (cov > 0.0f) blend(x, y, rgb, (uint8_t)(cov * alpha));
        }
    }
}

void Canvas::arc(const ArcStyle &s, float fill) {
    fill = clamp01(fill);
    if (fill <= 0.0f || s.sweep <= 0.0f) return;

    const float feather = s.feather > 0.0f ? s.feather : 1.0f;
    const float ro = s.rOuter, ri = s.rOuter - s.thickness;
    const float rm = ro - s.thickness * 0.5f, capR = s.thickness * 0.5f;
    const float fillSweep = s.sweep * fill;
    const bool fullRing = fillSweep >= 359.99f;
    const bool caps = s.roundCaps && !fullRing;

    float capX[2], capY[2];
    for (int i = 0; i < 2; i++) {
        float a = (s.start + s.dir * (i ? fillSweep : 0.0f)) * (float)M_PI / 180.0f;
        capX[i] = s.cx + rm * sinf(a);
        capY[i] = s.cy - rm * cosf(a);
    }

    const float outer = ro + feather, inner = ri - feather > 0 ? ri - feather : 0;
    const float outer2 = outer * outer, inner2 = inner * inner;
    int y0 = (int)floorf(s.cy - outer), y1 = (int)ceilf(s.cy + outer);
    int x0 = (int)floorf(s.cx - outer), x1 = (int)ceilf(s.cx + outer);
    if (y0 < 0) y0 = 0;
    if (x0 < 0) x0 = 0;
    if (y1 >= h) y1 = h - 1;
    if (x1 >= w) x1 = w - 1;

    for (int y = y0; y <= y1; y++) {
        float dy = y + 0.5f - s.cy;
        for (int x = x0; x <= x1; x++) {
            float dx = x + 0.5f - s.cx;
            float d2 = dx * dx + dy * dy;
            if (d2 > outer2 || d2 < inner2) continue;
            float d = sqrtf(d2);

            float cov = 0.0f, t = 0.0f;
            float radial = clamp01((ro - d) / feather + 0.5f) * clamp01((d - ri) / feather + 0.5f);
            if (radial > 0.0f) {
                float ang = wrap360(atan2f(dx, -dy) * 180.0f / (float)M_PI);
                float rel = s.dir > 0 ? wrap360(ang - s.start) : wrap360(s.start - ang);
                if (fullRing || rel <= fillSweep) {
                    cov = radial;
                    t = rel / s.sweep;
                }
            }
            if (caps && cov < 1.0f) {
                for (int i = 0; i < 2; i++) {
                    float cdx = x + 0.5f - capX[i], cdy = y + 0.5f - capY[i];
                    float c = clamp01((capR - sqrtf(cdx * cdx + cdy * cdy)) / feather + 0.5f);
                    if (c > cov) {
                        cov = c;
                        t = i ? fill : 0.0f;
                    }
                }
            }
            if (cov > 0.0f) blend(x, y, lerpColor(s.c0, s.c1, t), (uint8_t)(cov * s.alpha));
        }
    }
}

static const Glyph *findGlyph(const Font &font, uint16_t code) {
    for (uint16_t i = 0; i < font.count; i++) {
        if (font.glyphs[i].code == code) return &font.glyphs[i];
    }
    return nullptr;
}

int Canvas::textWidth(const Font &font, const char *text) const {
    int width = 0;
    for (const uint8_t *p = (const uint8_t *)text; *p; p++) {
        const Glyph *g = findGlyph(font, *p);
        if (g) width += g->advance;
    }
    return width;
}

void Canvas::text(const Font &font, const char *text, int x, int baseline, uint32_t rgb, Align align) {
    if (align == ALIGN_CENTER) x -= textWidth(font, text) / 2;
    else if (align == ALIGN_RIGHT) x -= textWidth(font, text);

    for (const uint8_t *p = (const uint8_t *)text; *p; p++) {
        const Glyph *g = findGlyph(font, *p);
        if (!g) continue;
        uint32_t n = g->offset;
        for (int gy = 0; gy < g->h; gy++) {
            for (int gx = 0; gx < g->w; gx++, n++) {
                uint8_t byte = font.bitmap[n >> 1];
                uint8_t a = (n & 1) ? (byte & 0x0F) : (byte >> 4);
                if (a) blend(x + g->xo + gx, baseline + g->yo + gy, rgb, a * 17);
            }
        }
        x += g->advance;
    }
}
