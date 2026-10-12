#include "display.h"

#include <esp_display_panel.hpp>

using namespace esp_panel::board;

static Board *board = nullptr;
static uint16_t *canvas = nullptr;

const char *initDisplay() {
    canvas = (uint16_t *)heap_caps_malloc(SCREEN_WIDTH * SCREEN_HEIGHT * 2, MALLOC_CAP_SPIRAM);
    if (!canvas) return "no PSRAM for frame buffer";
    memset(canvas, 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);

    board = new Board();
    Serial.println("display: init");
    if (!board->init()) return "board init failed";
    Serial.println("display: begin");
    if (!board->begin()) return "board begin failed";
    if (!board->getLCD()) return "no LCD driver";
    flushDisplay();
    if (board->getBacklight()) board->getBacklight()->on();
    else Serial.println("display: no backlight driver");
    Serial.println("display: ok");
    return nullptr;
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
