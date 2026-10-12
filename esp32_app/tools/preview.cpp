// Renders the dashboard on a desktop machine for design iteration.
// g++ -O2 -std=c++17 -I../src preview.cpp ../src/gfx.cpp ../src/dashboard.cpp -o preview && ./preview out_dir
#include <math.h>
#include <stdio.h>
#include <string>
#include <vector>

#include "dashboard.h"

static void writePPM(const std::string &path, const uint16_t *px, int w, int h) {
    FILE *f = fopen(path.c_str(), "wb");
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int i = 0; i < w * h; i++) {
        uint16_t p = px[i];
        int x = i % w - w / 2, y = i / w - h / 2;
        bool inside = x * x + y * y <= (w / 2) * (w / 2);
        unsigned char rgb[3] = {
            (unsigned char)(inside ? (p >> 11) << 3 : 40),
            (unsigned char)(inside ? ((p >> 5) & 0x3F) << 2 : 40),
            (unsigned char)(inside ? (p & 0x1F) << 3 : 40),
        };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void shot(const char *dir, const char *name, View view, Stats stats, bool online, Theme theme) {
    std::vector<uint16_t> fb(480 * 480);
    Canvas canvas(fb.data(), 480, 480);
    Dashboard d;
    d.setTheme(theme);
    d.setView(view);
    uint32_t t = 1;
    for (int i = 0; i < 400; i++) {
        if (online) d.setStats(stats, t);
        d.update(t += 16);
    }
    d.render(canvas);
    writePPM(std::string(dir) + "/" + name + ".ppm", fb.data(), 480, 480);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    Theme kraken = {0x2D7CFF, 0xB22CFF};
    Theme heat = {0x00D0FF, 0xFF3B30};
    shot(dir, "cpu", VIEW_CPU, {47.0f, 18.0f, 52.0f, 34.0f}, true, kraken);
    shot(dir, "cpu_hot", VIEW_CPU, {88.0f, 97.0f, 52.0f, 34.0f}, true, heat);
    shot(dir, "gpu", VIEW_GPU, {47.0f, 18.0f, 64.0f, 71.0f}, true, kraken);
    shot(dir, "dual", VIEW_DUAL, {47.0f, 18.0f, 64.0f, 71.0f}, true, kraken);
    shot(dir, "offline", VIEW_CPU, {NAN, NAN, NAN, NAN}, false, kraken);
    shot(dir, "dual_nogpu", VIEW_DUAL, {61.0f, 40.0f, NAN, NAN}, true, heat);
    return 0;
}
