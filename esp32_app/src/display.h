#pragma once

#include <Arduino.h>

// Screen dimensions
#define SCREEN_WIDTH  480
#define SCREEN_HEIGHT 480

void initDisplay();
void drawText(const char* text, int x, int y, uint16_t color);
void drawRect(int x, int y, int w, int h, uint16_t color);
void pushPixels(int x, int y, int w, int h, uint16_t* pixels);
void fillScreen(uint16_t color);
