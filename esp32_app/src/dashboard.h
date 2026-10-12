#pragma once

#include <stdint.h>
#include "gfx.h"

enum View { VIEW_CPU, VIEW_GPU, VIEW_DUAL, VIEW_LIQUID };

struct Stats {
    float cpuTemp, cpuLoad, gpuTemp, gpuLoad, liquidTemp, pumpRpm, fanRpm;
};

struct Theme {
    uint32_t c0, c1;
};

class Dashboard {
public:
    static const int VALUES = 7;

    Dashboard();

    void setStats(const Stats &stats, uint32_t nowMs);
    void setView(View v);
    void setTheme(const Theme &t);
    void setTrack(uint32_t rgb);
    void setTempColor(bool enabled);
    View getView() const { return view; }
    Theme getTheme() const { return theme; }
    uint32_t getTrack() const { return track; }
    bool getTempColor() const { return tempColor; }

    bool update(uint32_t nowMs);
    void render(Canvas &c);
    void renderMessage(Canvas &c, const char *title, const char *detail);

private:
    bool valid(int i) const;
    void drawGauge(Canvas &c, float start, float sweep, int dir, float fill);
    void renderSingle(Canvas &c, const char *label, int value, float minV, float maxV, const char *sub1, const char *sub2);
    void renderDual(Canvas &c);
    void renderHalf(Canvas &c, float start, int dir, int cx, const char *label, int temp, int load);

    float target[VALUES];
    float shown[VALUES];
    bool online;
    bool dirty;
    bool tempColor;
    uint32_t lastStatsMs;
    uint32_t lastUpdateMs;
    View view;
    Theme theme;
    uint32_t track;
};
