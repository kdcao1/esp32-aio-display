#include "display.h"

#include <esp_display_panel.hpp>

using namespace esp_panel::board;

static Board *board = nullptr;
static uint16_t *canvas = nullptr;

bool initDisplay() {
    canvas = (uint16_t *)heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    if (!canvas) {
        Serial.println("ERR no PSRAM for frame buffer");
        return false;
    }
    memset(canvas, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);

    board = new Board();
    if (!board->init() || !board->begin()) {
        Serial.println("ERR display init failed");
        return false;
    }
    flushDisplay();
    if (board->getBacklight()) board->getBacklight()->on();
    return true;
}

uint16_t *frameBuffer() {
    return canvas;
}

void flushDisplay() {
    if (board && board->getLCD()) {
        board->getLCD()->drawBitmap(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, (const uint8_t *)canvas);
    }
}

void setBrightness(int percent) {
    if (board && board->getBacklight()) board->getBacklight()->setBrightness(constrain(percent, 5, 100));
}
