#pragma once

#include <Arduino.h>
#include <FS.h>

bool initStorage();
fs::FS &gifFs();
bool onSdCard();
uint64_t freeBytes();
bool sanitizeName(const char *in, char *out, size_t n);
void gifPath(const char *name, char *out, size_t n);
bool gifExists(const char *name);
int listGifs(char names[][41], size_t *sizes, int max);
bool writeFile(const char *path, const uint8_t *data, size_t size);
