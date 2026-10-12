#pragma once

#include <Arduino.h>

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 480

const char *initDisplay();
uint16_t *frameBuffer();
uint16_t *visibleBuffer();
bool prepareSdPins();
void flushDisplay();
void setBrightness(int percent);
void setBacklight(bool on);
void resyncDisplay();
