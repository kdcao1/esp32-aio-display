#include <Arduino.h>
#include <Preferences.h>
#include <AnimatedGIF.h>
#include <esp_app_desc.h>

#include "display.h"
#include "dashboard.h"
#include "storage.h"

#define PROTOCOL_VERSION 6
#define MAX_GIF_BYTES (5 * 1024 * 1024)
#define MAX_GIFS 64
#define UPLOAD_CHUNK 4096
#define CHUNK_TIMEOUT_MS 1500
#define UPLOAD_RETRIES 8
#define SAVE_DELAY_MS 2000
#define FRAME_MS 33
#define FRAME_BYTES (SCREEN_WIDTH * SCREEN_HEIGHT * 2)
#define FS_MARGIN 16384

enum Mode { MODE_CPU, MODE_GPU, MODE_DUAL, MODE_GIF, MODE_LIQUID };

static Mode mode = MODE_CPU;
static int brightness = 100;
static Dashboard dash;
static Canvas *canvas = nullptr;
static Preferences prefs;

static AnimatedGIF gif;
static uint8_t *gifData = nullptr;
static uint16_t *gifTarget = nullptr;
static char currentGif[41] = "";
static bool smoothPref = false;
static int rotation = 0;
static bool screenOn = true, backlightOn = true;
static uint32_t sleepSec = 60, lastHostMs = 0;
static uint32_t cycleSec = 0, cycleAtMs = 0;
static uint32_t statFrames = 0, statDecodeMs = 0, statAtMs = 0;
static uint32_t lastRenderMs = 0;
static bool gifOpen = false;
static int gifOffX = 0, gifOffY = 0, gifScale = 1;
static uint32_t nextFrameMs = 0;

static const char *fatalError = nullptr;
static uint32_t saveAtMs = 0;
static char line[256];
static size_t lineLen = 0;

static uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t len) {
    crc = ~crc;
    while (len--) {
        crc ^= *data++;
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}

static Canvas &target() {
    canvas->setPixels(frameBuffer());
    return *canvas;
}

static void showMessage(const char *title, const char *detail) {
    dash.renderMessage(target(), title, detail);
    flushDisplay();
}

static void GIFDraw(GIFDRAW *p) {
    const int s = gifScale;
    int y = gifOffY + (p->iY + p->y) * s;
    if (y < 0 || y + s > SCREEN_HEIGHT) return;
    const uint8_t *src = p->pPixels;
    const uint16_t *pal = p->pPalette;
    int x0 = gifOffX + p->iX * s;
    int w = min(p->iWidth, (SCREEN_WIDTH - x0) / s);
    if (x0 < 0 || w <= 0) return;
    bool transparent = p->ucHasTransparency;
    uint8_t key = p->ucTransparent;
    if (rotation) {
        for (int i = 0; i < w; i++) {
            uint8_t c = src[i];
            if (transparent && c == key) continue;
            uint16_t v = pal[c];
            for (int dy = 0; dy < s; dy++) {
                for (int dx = 0; dx < s; dx++) {
                    int x = x0 + i * s + dx, yy = y + dy, tx = x, ty = yy;
                    if (rotation == 1) { tx = SCREEN_WIDTH - 1 - yy; ty = x; }
                    else if (rotation == 2) { tx = SCREEN_WIDTH - 1 - x; ty = SCREEN_HEIGHT - 1 - yy; }
                    else { tx = yy; ty = SCREEN_HEIGHT - 1 - x; }
                    gifTarget[ty * SCREEN_WIDTH + tx] = v;
                }
            }
        }
        return;
    }
    uint16_t *row = gifTarget + y * SCREEN_WIDTH + x0;
    if (s == 1) {
        for (int i = 0; i < w; i++) {
            uint8_t c = src[i];
            if (!transparent || c != key) row[i] = pal[c];
        }
        return;
    }
    uint16_t *next = row + SCREEN_WIDTH;
    for (int i = 0; i < w; i++) {
        uint8_t c = src[i];
        if (transparent && c == key) continue;
        uint16_t v = pal[c];
        row[2 * i] = row[2 * i + 1] = next[2 * i] = next[2 * i + 1] = v;
    }
}

static void closeGif() {
    if (gifOpen) gif.close();
    gifOpen = false;
    free(gifData);
    gifData = nullptr;
}

static bool pickGif(int offset) {
    static GifEntry entries[MAX_GIFS];
    int count = listGifs(entries, MAX_GIFS);
    if (!count) return false;
    int index = -1;
    for (int i = 0; i < count; i++) {
        if (!strcmp(entries[i].name, currentGif)) index = i;
    }
    index = index < 0 ? 0 : (index + offset + count) % count;
    strcpy(currentGif, entries[index].name);
    return true;
}

static void openGif() {
    closeGif();
    if (!currentGif[0] || !gifExists(currentGif)) {
        if (!pickGif(0)) {
            showMessage("NO GIFS YET", "UPLOAD ONE FROM THE PC APP");
            return;
        }
    }
    uint8_t variants = gifVariants(currentGif);
    bool smooth = smoothPref ? (variants & GIF_SMOOTH) : !(variants & GIF_SHARP);
    char path[64];
    gifPath(currentGif, smooth, path, sizeof(path));
    File f = gifFs().open(path, "r");
    size_t size = f ? f.size() : 0;
    gifData = size ? (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM) : nullptr;
    if (!gifData || f.read(gifData, size) != size) {
        if (f) f.close();
        closeGif();
        showMessage("GIF LOAD FAILED", currentGif);
        return;
    }
    f.close();
    if (!gif.open(gifData, size, GIFDraw)) {
        closeGif();
        showMessage("INVALID GIF", currentGif);
        return;
    }
    gifOpen = true;
    gifScale = (gif.getCanvasWidth() <= SCREEN_WIDTH / 2 && gif.getCanvasHeight() <= SCREEN_HEIGHT / 2) ? 2 : 1;
    gifOffX = (SCREEN_WIDTH - gif.getCanvasWidth() * gifScale) / 2;
    gifOffY = (SCREEN_HEIGHT - gif.getCanvasHeight() * gifScale) / 2;
    gifTarget = visibleBuffer();
    memset(gifTarget, 0, FRAME_BYTES);
    nextFrameMs = statAtMs = millis();
    statFrames = statDecodeMs = 0;
    cycleAtMs = millis() + cycleSec * 1000;
}

static void applyMode(Mode m) {
    mode = m;
    if (mode == MODE_GIF) {
        openGif();
        return;
    }
    closeGif();
    dash.setView(mode == MODE_GPU      ? VIEW_GPU
                 : mode == MODE_DUAL   ? VIEW_DUAL
                 : mode == MODE_LIQUID ? VIEW_LIQUID
                                       : VIEW_CPU);
    dash.render(target());
    flushDisplay();
}

static void writeSettings() {
    Theme t = dash.getTheme();
    prefs.putUChar("mode", (uint8_t)mode);
    prefs.putUInt("c0", t.c0);
    prefs.putUInt("c1", t.c1);
    prefs.putUInt("track", dash.getTrack());
    prefs.putBool("tempcolor", dash.getTempColor());
    prefs.putUChar("bright", (uint8_t)brightness);
    prefs.putString("gif", currentGif);
    prefs.putUInt("cycle", cycleSec);
    prefs.putBool("smooth", smoothPref);
    prefs.putUChar("rotation", (uint8_t)rotation);
    prefs.putUInt("sleep", sleepSec);
    resyncDisplay();
}

static void saveSettings() {
    saveAtMs = millis() + SAVE_DELAY_MS;
    if (!saveAtMs) saveAtMs = 1;
}

static float parseField(char *&cursor) {
    char *end = strchr(cursor, ',');
    if (end) *end = 0;
    float v = (*cursor == 0) ? NAN : strtof(cursor, nullptr);
    cursor = end ? end + 1 : cursor + strlen(cursor);
    return v;
}

static bool readExact(uint8_t *buf, size_t len, uint32_t timeoutMs) {
    size_t got = 0;
    uint32_t last = millis();
    while (got < len) {
        int n = Serial.available();
        if (n > 0) {
            got += Serial.read(buf + got, min((size_t)n, len - got));
            last = millis();
        } else if (millis() - last > timeoutMs) {
            return false;
        } else {
            delay(1);
        }
    }
    return true;
}

static void drainSerial() {
    uint32_t last = millis();
    while (millis() - last < 100) {
        if (Serial.available()) {
            Serial.read();
            last = millis();
        } else {
            delay(1);
        }
    }
}

static void handleUpload(char *args) {
    long size = strtol(args, &args, 10);
    uint32_t expected = (*args == ',') ? strtoul(args + 1, &args, 16) : 0;
    char name[41];
    char *variant = (*args == ',') ? strchr(args + 1, ',') : nullptr;
    if (variant) *variant++ = 0;
    if (*args != ',' || !sanitizeName(args + 1, name, sizeof(name))) strcpy(name, "gif");
    bool smooth = variant && toupper(*variant) == 'S';

    char path[64];
    gifPath(name, smooth, path, sizeof(path));
    long existing = 0;
    if (File old = gifFs().open(path, "r")) {
        existing = old.size();
        old.close();
    }
    uint64_t avail = freeBytes() + existing;
    long space = avail > (uint64_t)MAX_GIF_BYTES + FS_MARGIN ? MAX_GIF_BYTES : (long)avail - FS_MARGIN;
    if (size <= 0 || size > space) {
        Serial.printf("ERR SIZE %ld\n", space);
        return;
    }

    closeGif();
    uint8_t *data = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!data) {
        Serial.println("ERR MEM");
        applyMode(mode);
        return;
    }

    showMessage("RECEIVING GIF", "");
    Serial.println("READY");

    long received = 0;
    int retries = 0;
    const char *error = nullptr;
    while (received < size && !error) {
        size_t chunk = min((long)UPLOAD_CHUNK, size - received);
        uint8_t trailer[4];
        const char *reason = nullptr;
        if (!readExact(data + received, chunk, CHUNK_TIMEOUT_MS) || !readExact(trailer, 4, CHUNK_TIMEOUT_MS)) {
            reason = "TIMEOUT";
        } else {
            uint32_t sent = trailer[0] | (trailer[1] << 8) | (trailer[2] << 16) | ((uint32_t)trailer[3] << 24);
            if (crc32Update(0, data + received, chunk) != sent) reason = "CRC";
        }
        if (reason) {
            if (++retries > UPLOAD_RETRIES) {
                error = reason;
                break;
            }
            drainSerial();
            Serial.printf("NAK %ld %s\n", received, reason);
            continue;
        }
        received += chunk;
        Serial.printf("ACK %ld\n", received);
    }

    if (!error && crc32Update(0, data, size) != expected) error = "CRC";
    if (!error) {
        showMessage("SAVING GIF", "");
        bool flash = !onSdCard();
        if (flash) {
            delay(50);
            setBacklight(false);
        }
        bool ok = writeFile(path, data, size);
        if (flash) {
            resyncDisplay();
            setBacklight(true);
        }
        if (ok) strcpy(currentGif, name);
        else error = "WRITE";
    }
    free(data);

    if (error) Serial.printf("ERR %s\n", error);
    else Serial.printf("OK %d\n", retries);
    saveSettings();
    applyMode(mode);
}

static void handleCommand(char *cmd) {
    char *arg = strchr(cmd, ':');
    if (arg) *arg++ = 0;
    else arg = cmd + strlen(cmd);

    if (!strcmp(cmd, "PING")) {
        Serial.printf("PONG COOLERSCREEN %d\n", PROTOCOL_VERSION);
    } else if (!strcmp(cmd, "STAT")) {
        Stats s;
        s.cpuTemp = parseField(arg);
        s.cpuLoad = parseField(arg);
        s.gpuTemp = parseField(arg);
        s.gpuLoad = parseField(arg);
        s.liquidTemp = parseField(arg);
        s.pumpRpm = parseField(arg);
        s.fanRpm = parseField(arg);
        dash.setStats(s, millis());
    } else if (!strcmp(cmd, "TEMP")) {
        dash.setStats({strtof(arg, nullptr), NAN, NAN, NAN, NAN, NAN, NAN}, millis());
    } else if (!strcmp(cmd, "MODE")) {
        Mode m = mode;
        if (!strcmp(arg, "CPU") || !strcmp(arg, "TEMP")) m = MODE_CPU;
        else if (!strcmp(arg, "GPU")) m = MODE_GPU;
        else if (!strcmp(arg, "DUAL")) m = MODE_DUAL;
        else if (!strcmp(arg, "GIF")) m = MODE_GIF;
        else if (!strcmp(arg, "LIQUID")) m = MODE_LIQUID;
        if (m != mode || (m == MODE_GIF && !gifOpen)) applyMode(m);
        saveSettings();
    } else if (!strcmp(cmd, "THEME")) {
        char *second = strchr(arg, ',');
        if (second) {
            dash.setTheme({(uint32_t)strtoul(arg, nullptr, 16), (uint32_t)strtoul(second + 1, nullptr, 16)});
            saveSettings();
        }
    } else if (!strcmp(cmd, "COLORMODE")) {
        dash.setTempColor(!strcmp(arg, "TEMP"));
        saveSettings();
    } else if (!strcmp(cmd, "TRACK")) {
        dash.setTrack(strtoul(arg, nullptr, 16));
        saveSettings();
    } else if (!strcmp(cmd, "BRIGHT")) {
        brightness = constrain(atoi(arg), 5, 100);
        setBrightness(brightness);
        saveSettings();
    } else if (!strcmp(cmd, "UPLOAD")) {
        handleUpload(arg);
    } else if (!strcmp(cmd, "GIFLIST")) {
        static GifEntry entries[MAX_GIFS];
        int count = listGifs(entries, MAX_GIFS);
        for (int i = 0; i < count; i++) {
            const GifEntry &e = entries[i];
            Serial.printf("GIF %s %u %s%s\n", e.name, (unsigned)e.size, e.variants & GIF_SHARP ? "H" : "",
                          e.variants & GIF_SMOOTH ? "S" : "");
        }
        Serial.printf("END %llu %s %s %u\n", freeBytes(), onSdCard() ? "SD" : "FLASH", currentGif[0] ? currentGif : "-",
                      (unsigned)cycleSec);
    } else if (!strcmp(cmd, "GIFPLAY")) {
        char name[41];
        if (sanitizeName(arg, name, sizeof(name)) && gifExists(name)) {
            strcpy(currentGif, name);
            applyMode(MODE_GIF);
            saveSettings();
        }
    } else if (!strcmp(cmd, "GIFDEL")) {
        char name[41], path[64];
        if (sanitizeName(arg, name, sizeof(name))) {
            bool playing = !strcmp(name, currentGif);
            if (playing) closeGif();
            gifPath(name, false, path, sizeof(path));
            gifFs().remove(path);
            gifPath(name, true, path, sizeof(path));
            gifFs().remove(path);
            if (playing) {
                currentGif[0] = 0;
                if (mode == MODE_GIF) applyMode(MODE_GIF);
            }
            saveSettings();
        }
        Serial.println("DONE");
    } else if (!strcmp(cmd, "VERSION")) {
        const uint8_t *sha = esp_app_get_description()->app_elf_sha256;
        Serial.printf("VERSION %02x%02x%02x%02x%02x%02x%02x%02x %d\n", sha[0], sha[1], sha[2], sha[3], sha[4], sha[5],
                      sha[6], sha[7], PROTOCOL_VERSION);
    } else if (!strcmp(cmd, "ROTATE")) {
        int r = (atoi(arg) / 90) & 3;
        if (r != rotation) {
            rotation = r;
            canvas->setRotation(rotation);
            applyMode(mode);
            saveSettings();
        }
    } else if (!strcmp(cmd, "SCREEN")) {
        screenOn = strcmp(arg, "OFF") != 0;
    } else if (!strcmp(cmd, "SLEEP")) {
        sleepSec = strtoul(arg, nullptr, 10);
        saveSettings();
    } else if (!strcmp(cmd, "QUALITY")) {
        bool smooth = !strcmp(arg, "SMOOTH");
        if (smooth != smoothPref) {
            smoothPref = smooth;
            if (mode == MODE_GIF) applyMode(MODE_GIF);
            saveSettings();
        }
    } else if (!strcmp(cmd, "SDINFO")) {
        Serial.printf("SDINFO %s\n", sdStatus());
    } else if (!strcmp(cmd, "SDFORMAT")) {
        closeGif();
        showMessage("PREPARING SD CARD", "");
        formatSd();
        Serial.printf("SDINFO %s\n", sdStatus());
        applyMode(mode);
    } else if (!strcmp(cmd, "GIFCYCLE")) {
        cycleSec = strtoul(arg, nullptr, 10);
        cycleAtMs = millis() + cycleSec * 1000;
        saveSettings();
    }
}

static void processSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            line[lineLen] = 0;
            lineLen = 0;
            lastHostMs = millis();
            handleCommand(line);
            if (mode == MODE_GIF && !gifOpen) return;
        } else if (c != '\r' && lineLen < sizeof(line) - 1) {
            line[lineLen++] = c;
        }
    }
}

void setup() {
    size_t rxBuffer = Serial.setRxBufferSize(UPLOAD_CHUNK * 4);
    Serial.begin(921600);
    Serial.setTimeout(50);
    for (uint32_t start = millis(); !Serial && millis() - start < 3000;) delay(10);
    Serial.setDebugOutput(true);

    Serial.printf("CoolerScreen protocol %d, rx buffer %u\n", PROTOCOL_VERSION, rxBuffer);
    Serial.printf("chip %s rev %d, flash %u, PSRAM %u (free %u)\n", ESP.getChipModel(), ESP.getChipRevision(),
                  ESP.getFlashChipSize(), ESP.getPsramSize(), ESP.getFreePsram());

    fatalError = initDisplay();
    if (fatalError) return;
    initStorage();
    canvas = new Canvas(frameBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
    gif.begin(GIF_PALETTE_RGB565_LE);

    prefs.begin("cooler", false);
    brightness = prefs.getUChar("bright", 100);
    dash.setTheme({prefs.getUInt("c0", 0x2D7CFF), prefs.getUInt("c1", 0xB22CFF)});
    dash.setTrack(prefs.getUInt("track", 0x1C1C24));
    dash.setTempColor(prefs.getBool("tempcolor", false));
    prefs.getString("gif", currentGif, sizeof(currentGif));
    cycleSec = prefs.getUInt("cycle", 0);
    smoothPref = prefs.getBool("smooth", false);
    rotation = prefs.getUChar("rotation", 0) & 3;
    canvas->setRotation(rotation);
    sleepSec = prefs.getUInt("sleep", 60);
    setBrightness(brightness);
    applyMode((Mode)constrain(prefs.getUChar("mode", MODE_CPU), MODE_CPU, MODE_LIQUID));
}

void loop() {
    if (fatalError) {
        Serial.printf("ERR %s\n", fatalError);
        delay(2000);
        return;
    }
    processSerial();
    uint32_t now = millis();
    if (saveAtMs && (int32_t)(now - saveAtMs) >= 0) {
        saveAtMs = 0;
        writeSettings();
    }

    bool wantOn = screenOn && !(sleepSec && now - lastHostMs > sleepSec * 1000);
    if (wantOn != backlightOn) {
        backlightOn = wantOn;
        if (wantOn) {
            setBacklight(true);
            setBrightness(brightness);
            nextFrameMs = now;
        } else {
            setBacklight(false);
        }
    }
    if (!backlightOn) {
        delay(20);
        return;
    }

    if (mode != MODE_GIF) {
        if (now - lastRenderMs >= FRAME_MS && dash.update(now)) {
            lastRenderMs = now;
            dash.render(target());
            flushDisplay();
        }
        delay(5);
        return;
    }

    if (gifOpen && cycleSec && (int32_t)(now - cycleAtMs) >= 0) {
        if (pickGif(1)) openGif();
        cycleAtMs = now + cycleSec * 1000;
        return;
    }
    if (!gifOpen || (int32_t)(now - nextFrameMs) < 0) {
        delay(1);
        return;
    }
    int delayMs = 0;
    int result = gif.playFrame(false, &delayMs);
    if (result <= 0) gif.reset();
    uint32_t done = millis();
    statFrames++;
    statDecodeMs += done - now;
    if (done - statAtMs >= 10000) {
        Serial.printf("gif %s: %.1f fps, %lu ms/frame decode\n", currentGif, statFrames * 1000.0f / (done - statAtMs),
                      (unsigned long)(statDecodeMs / (statFrames ? statFrames : 1)));
        statAtMs = done;
        statFrames = statDecodeMs = 0;
    }
    nextFrameMs = now + max(delayMs, 10);
}
