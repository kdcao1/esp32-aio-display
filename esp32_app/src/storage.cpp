#include "storage.h"

#include <LittleFS.h>
#include <vfs_api.h>
#include <driver/sdmmc_host.h>
#include <esp_vfs_fat.h>
#include <sdmmc_cmd.h>

#include "display.h"

#define SD_MOUNT "/sdcard"
#define GIF_DIR "/gifs"

class SdFs : public fs::FS {
public:
    SdFs() : FS(FSImplPtr(new VFSImpl())) {}
    void attach(const char *mount) { _impl->mountpoint(mount); }
};

static SdFs sdFs;
static sdmmc_card_t *card = nullptr;
static bool sdOk = false;
static char status[64] = "NOCARD not probed";

static esp_err_t mountSd(int freqKhz, bool format) {
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.flags = SDMMC_HOST_FLAG_1BIT;
    host.max_freq_khz = freqKhz;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 1;
    slot.clk = GPIO_NUM_2;
    slot.cmd = GPIO_NUM_1;
    slot.d0 = GPIO_NUM_42;
    slot.d1 = slot.d2 = slot.d3 = slot.d4 = slot.d5 = slot.d6 = slot.d7 = GPIO_NUM_NC;
    slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;
    esp_vfs_fat_sdmmc_mount_config_t cfg = {};
    cfg.format_if_mount_failed = format;
    cfg.max_files = 5;
    cfg.allocation_unit_size = 32 * 1024;
    return esp_vfs_fat_sdmmc_mount(SD_MOUNT, &host, &slot, &cfg, &card);
}

static bool startSd(bool format) {
    if (sdOk) return true;
    if (!prepareSdPins()) {
        snprintf(status, sizeof(status), "NOCARD io expander unavailable");
        return false;
    }
    esp_err_t err = mountSd(SDMMC_FREQ_DEFAULT, format);
    if (err != ESP_OK && err != ESP_FAIL) err = mountSd(SDMMC_FREQ_PROBING, format);
    if (err == ESP_OK) {
        sdOk = true;
        sdFs.attach(SD_MOUNT);
        snprintf(status, sizeof(status), "OK %llu", (unsigned long long)card->csd.capacity * card->csd.sector_size);
    } else if (err == ESP_FAIL) {
        snprintf(status, sizeof(status), "UNREADABLE card is not FAT32");
    } else {
        snprintf(status, sizeof(status), "NOCARD %s", esp_err_to_name(err));
    }
    Serial.printf("sd: %s\n", status);
    return sdOk;
}

bool initStorage() {
    if (!LittleFS.begin(true)) Serial.println("ERR LittleFS mount failed");
    startSd(false);
    Serial.printf("storage: %s\n", sdOk ? "SD card" : "internal flash");
    if (!gifFs().exists(GIF_DIR)) gifFs().mkdir(GIF_DIR);
    return sdOk;
}

bool formatSd() {
    if (!sdOk) startSd(true);
    if (sdOk && !gifFs().exists(GIF_DIR)) gifFs().mkdir(GIF_DIR);
    return sdOk;
}

fs::FS &gifFs() {
    return sdOk ? (fs::FS &)sdFs : (fs::FS &)LittleFS;
}

bool onSdCard() {
    return sdOk;
}

const char *sdStatus() {
    return status;
}

uint64_t freeBytes() {
    if (!sdOk) return LittleFS.totalBytes() - LittleFS.usedBytes();
    uint64_t total = 0, free = 0;
    esp_vfs_fat_info(SD_MOUNT, &total, &free);
    return free;
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
