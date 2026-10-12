#pragma once

#include <Arduino.h>
#include <FS.h>

bool initStorage();
fs::FS &gifFs();
bool onSdCard();
const char *sdStatus();
bool formatSd();
uint64_t freeBytes();
bool sanitizeName(const char *in, char *out, size_t n);
struct GifEntry {
    char name[41];
    size_t size;
    uint8_t variants;
};

enum { GIF_SHARP = 1, GIF_SMOOTH = 2 };

void gifPath(const char *name, bool smooth, char *out, size_t n);
uint8_t gifVariants(const char *name);
bool gifExists(const char *name);
int listGifs(GifEntry *entries, int max);
bool writeFile(const char *path, const uint8_t *data, size_t size);
