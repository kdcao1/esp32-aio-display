#include <Arduino.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <AnimatedGIF.h>

#include "display.h"
#include "dashboard.h"

#define PROTOCOL_VERSION 2
#define GIF_PATH "/anim.gif"
#define UPLOAD_CHUNK 4096
#define UPLOAD_TIMEOUT_MS 5000
#define FS_MARGIN 16384

enum Mode { MODE_CPU, MODE_GPU, MODE_DUAL, MODE_GIF };

static Mode mode = MODE_CPU;
static int brightness = 100;
static Dashboard dash;
static Canvas *canvas = nullptr;
static Preferences prefs;

static AnimatedGIF gif;
static uint8_t *gifData = nullptr;
static bool gifOpen = false;
static int gifOffX = 0, gifOffY = 0;
static uint32_t nextFrameMs = 0;

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
    int y = gifOffY + p->iY + p->y;
    if (y < 0 || y >= SCREEN_HEIGHT) return;
    uint16_t *row = frameBuffer() + y * SCREEN_WIDTH;
    const uint8_t *src = p->pPixels;
    const uint16_t *pal = p->pPalette;
    int x0 = gifOffX + p->iX;
    for (int i = 0; i < p->iWidth; i++) {
        int x = x0 + i;
        uint8_t c = src[i];
        if (x < 0 || x >= SCREEN_WIDTH) continue;
        if (p->ucHasTransparency && c == p->ucTransparent) continue;
        row[x] = pal[c];
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
    gifOffX = (SCREEN_WIDTH - gif.getCanvasWidth()) / 2;
    gifOffY = (SCREEN_HEIGHT - gif.getCanvasHeight()) / 2;
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
    dash.setView(mode == MODE_GPU ? VIEW_GPU : mode == MODE_DUAL ? VIEW_DUAL : VIEW_CPU);
    dash.render(*canvas);
    flushDisplay();
}

static void saveSettings() {
    Theme t = dash.getTheme();
    prefs.putUChar("mode", (uint8_t)mode);
    prefs.putUInt("c0", t.c0);
    prefs.putUInt("c1", t.c1);
    prefs.putUChar("bright", (uint8_t)brightness);
}

static float parseField(char *&cursor) {
    char *end = strchr(cursor, ',');
    if (end) *end = 0;
    float v = (*cursor == 0) ? NAN : strtof(cursor, nullptr);
    cursor = end ? end + 1 : cursor + strlen(cursor);
    return v;
}

static bool readExact(uint8_t *buf, size_t len) {
    size_t got = 0;
    uint32_t last = millis();
    while (got < len) {
        int n = Serial.available();
        if (n > 0) {
            got += Serial.readBytes(buf + got, min((size_t)n, len - got));
            last = millis();
        } else if (millis() - last > UPLOAD_TIMEOUT_MS) {
            return false;
        } else {
            delay(1);
        }
    }
    return true;
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
    LittleFS.remove(GIF_PATH);
    File f = LittleFS.open(GIF_PATH, "w");
    if (!f) {
        Serial.println("ERR FS");
        applyMode(mode);
        return;
    }

    showMessage("RECEIVING GIF", "");
    Serial.println("READY");

    static uint8_t buf[UPLOAD_CHUNK];
    uint32_t crc = 0;
    long received = 0;
    const char *error = nullptr;
    while (received < size) {
        size_t chunk = min((long)UPLOAD_CHUNK, size - received);
        if (!readExact(buf, chunk)) {
            error = "TIMEOUT";
            break;
        }
        if (f.write(buf, chunk) != chunk) {
            error = "WRITE";
            break;
        }
        crc = crc32Update(crc, buf, chunk);
        received += chunk;
        Serial.printf("ACK %ld\n", received);
    }
    f.close();

    if (!error && crc != expected) error = "CRC";
    if (error) {
        LittleFS.remove(GIF_PATH);
        Serial.printf("ERR %s\n", error);
    } else {
        Serial.println("OK");
    }
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
        dash.setStats(s, millis());
    } else if (!strcmp(cmd, "TEMP")) {
        dash.setStats({strtof(arg, nullptr), NAN, NAN, NAN}, millis());
    } else if (!strcmp(cmd, "MODE")) {
        Mode m = mode;
        if (!strcmp(arg, "CPU") || !strcmp(arg, "TEMP")) m = MODE_CPU;
        else if (!strcmp(arg, "GPU")) m = MODE_GPU;
        else if (!strcmp(arg, "DUAL")) m = MODE_DUAL;
        else if (!strcmp(arg, "GIF")) m = MODE_GIF;
        if (m != mode || (m == MODE_GIF && !gifOpen)) applyMode(m);
        saveSettings();
    } else if (!strcmp(cmd, "THEME")) {
        char *second = strchr(arg, ',');
        if (second) {
            dash.setTheme({(uint32_t)strtoul(arg, nullptr, 16), (uint32_t)strtoul(second + 1, nullptr, 16)});
            saveSettings();
        }
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
    Serial.setRxBufferSize(UPLOAD_CHUNK * 2);
    Serial.begin(921600);
    Serial.setTimeout(50);

    if (!LittleFS.begin(true)) Serial.println("ERR LittleFS mount failed");
    if (!initDisplay()) {
        while (true) delay(1000);
    }
    canvas = new Canvas(frameBuffer(), SCREEN_WIDTH, SCREEN_HEIGHT);
    gif.begin(GIF_PALETTE_RGB565_LE);

    prefs.begin("cooler", false);
    brightness = prefs.getUChar("bright", 100);
    dash.setTheme({prefs.getUInt("c0", 0x2D7CFF), prefs.getUInt("c1", 0xB22CFF)});
    setBrightness(brightness);
    applyMode((Mode)constrain(prefs.getUChar("mode", MODE_CPU), MODE_CPU, MODE_GIF));
}

void loop() {
    processSerial();
    uint32_t now = millis();

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
