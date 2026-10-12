#pragma once

#include <stdint.h>
#include "gfx.h"

enum View { VIEW_CPU, VIEW_GPU, VIEW_DUAL };

struct Stats {
    float cpuTemp, cpuLoad, gpuTemp, gpuLoad;
};

struct Theme {
    uint32_t c0, c1;
};

class Dashboard {
public:
    Dashboard();

    void setStats(const Stats &stats, uint32_t nowMs);
    void setView(View v);
    void setTheme(const Theme &t);
    View getView() const { return view; }
    Theme getTheme() const { return theme; }

    bool update(uint32_t nowMs);
    void render(Canvas &c);
    void renderMessage(Canvas &c, const char *title, const char *detail);

private:
    void renderSingle(Canvas &c, const char *label, int temp, int load);
    void renderDual(Canvas &c);
    void renderGaugeHalf(Canvas &c, float start, int dir, float fill, int cx, const char *label, int temp, int load);

    float target[4];
    float shown[4];
    bool online;
    bool dirty;
    uint32_t lastStatsMs;
    uint32_t lastUpdateMs;
    View view;
    Theme theme;
};
