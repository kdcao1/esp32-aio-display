#pragma once

#include <Arduino.h>

#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 480

bool initDisplay();
uint16_t *frameBuffer();
void flushDisplay();
void setBrightness(int percent);
