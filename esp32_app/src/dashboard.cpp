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
static const float TEMP_MAX = 100.0f;

enum { CPU_T, CPU_L, GPU_T, GPU_L };

Dashboard::Dashboard()
    : online(false), dirty(true), lastStatsMs(0), lastUpdateMs(0), view(VIEW_CPU), theme{0x2D7CFF, 0xB22CFF}, track(0x1C1C24) {
    for (int i = 0; i < 4; i++) {
        target[i] = NAN;
        shown[i] = 0.0f;
    }
}

void Dashboard::setStats(const Stats &s, uint32_t nowMs) {
    target[CPU_T] = s.cpuTemp;
    target[CPU_L] = s.cpuLoad;
    target[GPU_T] = s.gpuTemp;
    target[GPU_L] = s.gpuLoad;
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

bool Dashboard::update(uint32_t nowMs) {
    float dt = lastUpdateMs ? (float)(nowMs - lastUpdateMs) : 16.0f;
    lastUpdateMs = nowMs;

    if (online && nowMs - lastStatsMs > OFFLINE_MS) {
        online = false;
        dirty = true;
    }

    float k = 1.0f - expf(-dt / 180.0f);
    for (int i = 0; i < 4; i++) {
        float goal = (online && !isnan(target[i])) ? target[i] : 0.0f;
        float diff = goal - shown[i];
        if (fabsf(diff) < 0.02f) {
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

void Dashboard::render(Canvas &c) {
    c.clear(0);
    if (view == VIEW_DUAL) {
        renderDual(c);
        return;
    }
    int t = view == VIEW_CPU ? CPU_T : GPU_T;
    int l = view == VIEW_CPU ? CPU_L : GPU_L;
    renderSingle(c, view == VIEW_CPU ? "CPU" : "GPU", t, l);
}

void Dashboard::renderSingle(Canvas &c, const char *label, int t, int l) {
    bool tempValid = online && !isnan(target[t]);
    bool loadValid = online && !isnan(target[l]);
    float fill = shown[t] / TEMP_MAX;

    ArcStyle gauge = {CX, CY, 232, 20, 220, 280, 1, track, track, 255, 1.0f, true};
    c.arc(gauge, 1.0f);

    ArcStyle glow = gauge;
    glow.rOuter = 240;
    glow.thickness = 36;
    glow.feather = 9.0f;
    glow.alpha = 90;
    glow.c0 = theme.c0;
    glow.c1 = theme.c1;
    c.arc(glow, fill);

    ArcStyle value = gauge;
    value.c0 = theme.c0;
    value.c1 = theme.c1;
    c.arc(value, fill);

    c.text(font_small, "0", CX - 100, CY + 205, DIM, ALIGN_CENTER);
    c.text(font_small, "100", CX + 100, CY + 205, DIM, ALIGN_CENTER);

    c.text(font_label, label, CX, CY - 92, MUTED, ALIGN_CENTER);

    char num[8];
    formatValue(num, sizeof(num), tempValid, shown[t]);
    int baseline = CY + font_big.capHeight / 2 + 6;
    c.text(font_big, num, CX, baseline, WHITE, ALIGN_CENTER);
    int numRight = CX + c.textWidth(font_big, num) / 2;
    c.text(font_unit, "\xB0", numRight + 2, baseline - font_big.capHeight + font_unit.capHeight, MUTED);

    char sub[24];
    if (!online) {
        snprintf(sub, sizeof(sub), "WAITING FOR PC");
    } else if (loadValid) {
        snprintf(sub, sizeof(sub), "LOAD %d%%", (int)lroundf(shown[l]));
    } else {
        sub[0] = 0;
    }
    c.text(font_small, sub, CX, CY + 118, MUTED, ALIGN_CENTER);
}

void Dashboard::renderGaugeHalf(Canvas &c, float start, int dir, float fill, int cx, const char *label, int t, int l) {
    ArcStyle gauge = {CX, CY, 232, 20, start, 140, dir, track, track, 255, 1.0f, true};
    c.arc(gauge, 1.0f);

    ArcStyle glow = gauge;
    glow.rOuter = 240;
    glow.thickness = 36;
    glow.feather = 9.0f;
    glow.alpha = 90;
    glow.c0 = theme.c0;
    glow.c1 = theme.c1;
    c.arc(glow, fill);

    ArcStyle value = gauge;
    value.c0 = theme.c0;
    value.c1 = theme.c1;
    c.arc(value, fill);

    bool tempValid = online && !isnan(target[t]);
    bool loadValid = online && !isnan(target[l]);

    c.text(font_label, label, cx, CY - 70, MUTED, ALIGN_CENTER);

    char num[8];
    formatValue(num, sizeof(num), tempValid, shown[t]);
    int baseline = CY + font_mid.capHeight / 2 + 4;
    c.text(font_mid, num, cx, baseline, WHITE, ALIGN_CENTER);
    int numRight = cx + c.textWidth(font_mid, num) / 2;
    c.text(font_small, "\xB0", numRight + 2, baseline - font_mid.capHeight + font_small.capHeight, MUTED);

    if (loadValid) {
        char sub[12];
        snprintf(sub, sizeof(sub), "%d%%", (int)lroundf(shown[l]));
        c.text(font_small, sub, cx, CY + 92, MUTED, ALIGN_CENTER);
    }
}

void Dashboard::renderDual(Canvas &c) {
    renderGaugeHalf(c, 200, 1, shown[CPU_T] / TEMP_MAX, CX - 100, "CPU", CPU_T, CPU_L);
    renderGaugeHalf(c, 160, -1, shown[GPU_T] / TEMP_MAX, CX + 100, "GPU", GPU_T, GPU_L);
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
