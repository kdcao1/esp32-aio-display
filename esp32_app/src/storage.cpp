#include "storage.h"

#include <LittleFS.h>
#include <SD_MMC.h>

#include "display.h"

#define SD_CLK 2
#define SD_CMD 1
#define SD_D0 42
#define GIF_DIR "/gifs"

static bool sdOk = false;

bool initStorage() {
    if (!LittleFS.begin(true)) Serial.println("ERR LittleFS mount failed");
    if (prepareSdPins() && SD_MMC.setPins(SD_CLK, SD_CMD, SD_D0) && SD_MMC.begin("/sdcard", true, false, SDMMC_FREQ_DEFAULT)) {
        sdOk = SD_MMC.cardType() != CARD_NONE;
    }
    Serial.printf("storage: %s\n", sdOk ? "SD card" : "internal flash");
    if (!gifFs().exists(GIF_DIR)) gifFs().mkdir(GIF_DIR);
    return sdOk;
}

fs::FS &gifFs() {
    return sdOk ? (fs::FS &)SD_MMC : (fs::FS &)LittleFS;
}

bool onSdCard() {
    return sdOk;
}

uint64_t freeBytes() {
    return sdOk ? SD_MMC.totalBytes() - SD_MMC.usedBytes() : LittleFS.totalBytes() - LittleFS.usedBytes();
}

bool sanitizeName(const char *in, char *out, size_t n) {
    size_t len = 0;
    for (; *in && len < n - 1; in++) {
        char c = tolower(*in);
        if (isalnum((unsigned char)c) || c == '-' || c == '_') out[len++] = c;
    }
    out[len] = 0;
    return len > 0;
}

void gifPath(const char *name, char *out, size_t n) {
    snprintf(out, n, GIF_DIR "/%s.gif", name);
}

bool gifExists(const char *name) {
    char path[64];
    gifPath(name, path, sizeof(path));
    return gifFs().exists(path);
}

int listGifs(char names[][41], size_t *sizes, int max) {
    File dir = gifFs().open(GIF_DIR);
    int count = 0;
    if (!dir || !dir.isDirectory()) return 0;
    for (File f = dir.openNextFile(); f && count < max; f = dir.openNextFile()) {
        const char *name = f.name();
        const char *slash = strrchr(name, '/');
        if (slash) name = slash + 1;
        size_t len = strlen(name);
        if (!f.isDirectory() && len > 4 && len - 4 <= 40 && !strcasecmp(name + len - 4, ".gif")) {
            memcpy(names[count], name, len - 4);
            names[count][len - 4] = 0;
            sizes[count] = f.size();
            count++;
        }
        f.close();
    }
    for (int i = 1; i < count; i++) {
        for (int j = i; j > 0 && strcmp(names[j - 1], names[j]) > 0; j--) {
            char tmp[41];
            strcpy(tmp, names[j]);
            strcpy(names[j], names[j - 1]);
            strcpy(names[j - 1], tmp);
            size_t t = sizes[j];
            sizes[j] = sizes[j - 1];
            sizes[j - 1] = t;
        }
    }
    return count;
}

bool writeFile(const char *path, const uint8_t *data, size_t size) {
    static uint8_t chunk[16384];
    gifFs().remove(path);
    File f = gifFs().open(path, "w");
    if (!f) return false;
    size_t written = 0;
    while (written < size) {
        size_t n = min(sizeof(chunk), size - written);
        memcpy(chunk, data + written, n);
        if (f.write(chunk, n) != n) break;
        written += n;
    }
    f.close();
    if (written != size) gifFs().remove(path);
    return written == size;
}
