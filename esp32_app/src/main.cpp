#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AnimatedGIF.h>

#include "display.h"
#include "dashboard.h"

#define PROTOCOL_VERSION 4
#define GIF_PATH "/anim.gif"
#define UPLOAD_CHUNK 4096
#define CHUNK_TIMEOUT_MS 1500
#define UPLOAD_RETRIES 8
#define SAVE_DELAY_MS 2000
#define FS_MARGIN 16384

enum Mode { MODE_CPU, MODE_GPU, MODE_DUAL, MODE_GIF, MODE_LIQUID };

static Mode mode = MODE_CPU;
static int brightness = 100;
static Dashboard dash;
static Canvas *canvas = nullptr;
static Preferences prefs;

static AnimatedGIF gif;
static uint8_t *gifData = nullptr;
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

static void showMessage(const char *title, const char *detail) {
    dash.renderMessage(*canvas, title, detail);
    flushDisplay();
}

static void GIFDraw(GIFDRAW *p) {
    const int s = gifScale;
    int y0 = gifOffY + (p->iY + p->y) * s;
    const uint8_t *src = p->pPixels;
    const uint16_t *pal = p->pPalette;
    int x0 = gifOffX + p->iX * s;
    for (int dy = 0; dy < s; dy++) {
        int y = y0 + dy;
        if (y < 0 || y >= SCREEN_HEIGHT) continue;
        uint16_t *row = frameBuffer() + y * SCREEN_WIDTH;
        for (int i = 0; i < p->iWidth; i++) {
            uint8_t c = src[i];
            if (p->ucHasTransparency && c == p->ucTransparent) continue;
            for (int dx = 0; dx < s; dx++) {
                int x = x0 + i * s + dx;
                if (x >= 0 && x < SCREEN_WIDTH) row[x] = pal[c];
            }
        }
    }
}

static void closeGif() {
    if (gifOpen) gif.close();
    gifOpen = false;
    free(gifData);
    gifData = nullptr;
}

static void openGif() {
    closeGif();
    File f = LittleFS.open(GIF_PATH, "r");
    if (!f) {
        showMessage("NO GIF UPLOADED", "UPLOAD ONE FROM THE PC APP");
        return;
    }
    size_t size = f.size();
    gifData = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!gifData || f.read(gifData, size) != size) {
        f.close();
        closeGif();
        showMessage("GIF LOAD FAILED", "");
        return;
    }
    f.close();
    if (!gif.open(gifData, size, GIFDraw)) {
        closeGif();
        showMessage("INVALID GIF", "");
        return;
    }
    gifOpen = true;
    gifScale = (gif.getCanvasWidth() <= SCREEN_WIDTH / 2 && gif.getCanvasHeight() <= SCREEN_HEIGHT / 2) ? 2 : 1;
    gifOffX = (SCREEN_WIDTH - gif.getCanvasWidth() * gifScale) / 2;
    gifOffY = (SCREEN_HEIGHT - gif.getCanvasHeight() * gifScale) / 2;
    canvas->clear(0);
    nextFrameMs = millis();
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
    dash.render(*canvas);
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

static bool writeGifFile(const uint8_t *data, size_t size) {
    LittleFS.remove(GIF_PATH);
    File f = LittleFS.open(GIF_PATH, "w");
    if (!f) return false;
    size_t written = 0;
    while (written < size) {
        size_t n = f.write(data + written, min((size_t)32768, size - written));
        if (n == 0) break;
        written += n;
    }
    f.close();
    if (written != size) LittleFS.remove(GIF_PATH);
    return written == size;
}

static void handleUpload(char *args) {
    long size = strtol(args, &args, 10);
    uint32_t expected = (*args == ',') ? strtoul(args + 1, nullptr, 16) : 0;

    long existing = 0;
    if (File old = LittleFS.open(GIF_PATH, "r")) {
        existing = old.size();
        old.close();
    }
    long space = (long)(LittleFS.totalBytes() - LittleFS.usedBytes()) + existing - FS_MARGIN;
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
        delay(50);
        setBacklight(false);
        bool ok = writeGifFile(data, size);
        resyncDisplay();
        setBacklight(true);
        if (!ok) error = "WRITE";
    }
    free(data);

    if (error) Serial.printf("ERR %s\n", error);
    else Serial.printf("OK %d\n", retries);
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
    }
}

static void processSerial() {
    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n') {
            line[lineLen] = 0;
            lineLen = 0;
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

    if (!LittleFS.begin(true)) Serial.println("ERR LittleFS mount failed");
    fatalError = initDisplay();
    if (fatalError) return;
    canvas = new Canvas(frameBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
    gif.begin(GIF_PALETTE_RGB565_LE);

    prefs.begin("cooler", false);
    brightness = prefs.getUChar("bright", 100);
    dash.setTheme({prefs.getUInt("c0", 0x2D7CFF), prefs.getUInt("c1", 0xB22CFF)});
    dash.setTrack(prefs.getUInt("track", 0x1C1C24));
    dash.setTempColor(prefs.getBool("tempcolor", false));
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

    if (mode != MODE_GIF) {
        if (dash.update(now)) {
            dash.render(*canvas);
            flushDisplay();
        }
        delay(5);
        return;
    }

    if (!gifOpen || (int32_t)(now - nextFrameMs) < 0) {
        delay(1);
        return;
    }
    int delayMs = 0;
    int result = gif.playFrame(false, &delayMs);
    flushDisplay();
    if (result <= 0) gif.reset();
    nextFrameMs = now + max(delayMs, 20);
}
