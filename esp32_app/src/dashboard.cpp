#include "dashboard.h"

#include <math.h>
#include <stdio.h>

#include "fonts/font_big.h"
#include "fonts/font_mid.h"
#include "fonts/font_unit.h"
#include "fonts/font_label.h"
#include "fonts/font_small.h"

static const int CX = 240, CY = 240;
static const uint32_t MUTED = 0x8E8E9C;
static const uint32_t DIM = 0x4A4A56;
static const uint32_t WHITE = 0xFFFFFF;
static const uint32_t OFFLINE_MS = 5000;

enum { CPU_T, CPU_L, GPU_T, GPU_L, LIQ_T, PUMP, FAN };

Dashboard::Dashboard()
    : online(false), dirty(true), tempColor(false), lastStatsMs(0), lastUpdateMs(0), view(VIEW_CPU),
      theme{0x2D7CFF, 0xB22CFF}, track(0x1C1C24) {
    for (int i = 0; i < VALUES; i++) {
        target[i] = NAN;
        shown[i] = 0.0f;
    }
}

void Dashboard::setStats(const Stats &s, uint32_t nowMs) {
    const float values[VALUES] = {s.cpuTemp, s.cpuLoad, s.gpuTemp, s.gpuLoad, s.liquidTemp, s.pumpRpm, s.fanRpm};
    for (int i = 0; i < VALUES; i++) {
        if (isnan(values[i]) != isnan(target[i])) dirty = true;
        target[i] = values[i];
    }
    lastStatsMs = nowMs;
    if (!online) dirty = true;
    online = true;
}

void Dashboard::setView(View v) {
    if (v != view) dirty = true;
    view = v;
}

void Dashboard::setTheme(const Theme &t) {
    theme = t;
    dirty = true;
}

void Dashboard::setTrack(uint32_t rgb) {
    track = rgb;
    dirty = true;
}

void Dashboard::setTempColor(bool enabled) {
    tempColor = enabled;
    dirty = true;
}

bool Dashboard::valid(int i) const {
    return online && !isnan(target[i]);
}

bool Dashboard::update(uint32_t nowMs) {
    float dt = lastUpdateMs ? (float)(nowMs - lastUpdateMs) : 16.0f;
    lastUpdateMs = nowMs;

    if (online && nowMs - lastStatsMs > OFFLINE_MS) {
        online = false;
        dirty = true;
    }

    float k = 1.0f - expf(-dt / 180.0f);
    for (int i = 0; i < VALUES; i++) {
        float goal = valid(i) ? target[i] : 0.0f;
        float diff = goal - shown[i];
        if (i >= PUMP || fabsf(diff) < 0.02f) {
            if (diff != 0.0f) dirty = true;
            shown[i] = goal;
        } else {
            shown[i] += diff * k;
            dirty = true;
        }
    }

    bool redraw = dirty;
    dirty = false;
    return redraw;
}

static void formatValue(char *buf, size_t n, bool valid, float v) {
    if (valid) snprintf(buf, n, "%d", (int)lroundf(v));
    else snprintf(buf, n, "--");
}

void Dashboard::drawGauge(Canvas &c, float start, float sweep, int dir, float fill) {
    ArcStyle gauge = {CX, CY, 232, 20, start, sweep, dir, track, track, 255, 1.0f, true};
    c.arc(gauge, 1.0f);

    uint32_t c0 = theme.c0, c1 = theme.c1;
    if (tempColor) c0 = c1 = lerpColor(theme.c0, theme.c1, fill);

    ArcStyle glow = gauge;
    glow.rOuter = 240;
    glow.thickness = 36;
    glow.feather = 9.0f;
    glow.alpha = 90;
    glow.c0 = c0;
    glow.c1 = c1;
    c.arc(glow, fill);

    ArcStyle value = gauge;
    value.c0 = c0;
    value.c1 = c1;
    c.arc(value, fill);
}

void Dashboard::render(Canvas &c) {
    c.clear(0);
    char sub1[24] = "", sub2[24] = "";
    switch (view) {
    case VIEW_DUAL:
        renderDual(c);
        return;
    case VIEW_LIQUID:
        if (valid(PUMP)) snprintf(sub1, sizeof(sub1), "PUMP %d RPM", (int)shown[PUMP]);
        if (valid(FAN)) snprintf(sub2, sizeof(sub2), "FAN %d RPM", (int)shown[FAN]);
        renderSingle(c, "LIQUID", LIQ_T, 20, 60, sub1, sub2);
        return;
    case VIEW_GPU:
        if (valid(GPU_L)) snprintf(sub1, sizeof(sub1), "LOAD %d%%", (int)lroundf(shown[GPU_L]));
        renderSingle(c, "GPU", GPU_T, 0, 100, sub1, sub2);
        return;
    default:
        if (valid(CPU_L)) snprintf(sub1, sizeof(sub1), "LOAD %d%%", (int)lroundf(shown[CPU_L]));
        if (valid(FAN)) snprintf(sub2, sizeof(sub2), "FAN %d RPM", (int)shown[FAN]);
        renderSingle(c, "CPU", CPU_T, 0, 100, sub1, sub2);
        return;
    }
}

void Dashboard::renderSingle(Canvas &c, const char *label, int t, float minV, float maxV, const char *sub1,
                             const char *sub2) {
    drawGauge(c, 220, 280, 1, (shown[t] - minV) / (maxV - minV));

    char scale[8];
    snprintf(scale, sizeof(scale), "%d", (int)minV);
    c.text(font_small, scale, CX - 100, CY + 205, DIM, ALIGN_CENTER);
    snprintf(scale, sizeof(scale), "%d", (int)maxV);
    c.text(font_small, scale, CX + 100, CY + 205, DIM, ALIGN_CENTER);

    c.text(font_label, label, CX, CY - 92, MUTED, ALIGN_CENTER);

    char num[8];
    formatValue(num, sizeof(num), valid(t), shown[t]);
    int baseline = CY + font_big.capHeight / 2 + 6;
    c.text(font_big, num, CX, baseline, WHITE, ALIGN_CENTER);
    int numRight = CX + c.textWidth(font_big, num) / 2;
    c.text(font_unit, "\xB0", numRight + 2, baseline - font_big.capHeight + font_unit.capHeight, MUTED);

    if (!online) {
        c.text(font_small, "WAITING FOR PC", CX, CY + 118, MUTED, ALIGN_CENTER);
        return;
    }
    c.text(font_small, sub1, CX, CY + 118, MUTED, ALIGN_CENTER);
    c.text(font_small, sub2, CX, CY + 146, MUTED, ALIGN_CENTER);
}

void Dashboard::renderHalf(Canvas &c, float start, int dir, int cx, const char *label, int t, int l) {
    drawGauge(c, start, 140, dir, shown[t] / 100.0f);

    c.text(font_label, label, cx, CY - 70, MUTED, ALIGN_CENTER);

    char num[8];
    formatValue(num, sizeof(num), valid(t), shown[t]);
    int baseline = CY + font_mid.capHeight / 2 + 4;
    c.text(font_mid, num, cx, baseline, WHITE, ALIGN_CENTER);
    int numRight = cx + c.textWidth(font_mid, num) / 2;
    c.text(font_small, "\xB0", numRight + 2, baseline - font_mid.capHeight + font_small.capHeight, MUTED);

    if (valid(l)) {
        char sub[12];
        snprintf(sub, sizeof(sub), "%d%%", (int)lroundf(shown[l]));
        c.text(font_small, sub, cx, CY + 92, MUTED, ALIGN_CENTER);
    }
}

void Dashboard::renderDual(Canvas &c) {
    renderHalf(c, 200, 1, CX - 100, "CPU", CPU_T, CPU_L);
    renderHalf(c, 160, -1, CX + 100, "GPU", GPU_T, GPU_L);
    c.vline(CX, CY - 60, CY + 60, 0x2A2A33, 255);
    if (!online) c.text(font_small, "WAITING FOR PC", CX, CY + 175, MUTED, ALIGN_CENTER);
}

void Dashboard::renderMessage(Canvas &c, const char *title, const char *detail) {
    c.clear(0);
    ArcStyle ring = {CX, CY, 232, 20, 0, 360, 1, theme.c0, theme.c1, 255, 1.0f, false};
    ring.alpha = 60;
    c.arc(ring, 1.0f);
    c.text(font_label, title, CX, CY + 4, WHITE, ALIGN_CENTER);
    if (detail && *detail) c.text(font_small, detail, CX, CY + 40, MUTED, ALIGN_CENTER);
}
