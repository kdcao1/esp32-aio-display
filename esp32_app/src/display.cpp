#include "display.h"

#include <esp_display_panel.hpp>
#include <esp_lcd_panel_rgb.h>

using namespace esp_panel::board;

static Board *board = nullptr;
static uint16_t *buffers[2] = {nullptr, nullptr};
static int front = 0;
static SemaphoreHandle_t vsync = nullptr;

IRAM_ATTR static bool onRefreshFinish(void *) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(vsync, &woken);
    return woken == pdTRUE;
}

const char *initDisplay() {
    vsync = xSemaphoreCreateBinary();
    board = new Board();
    Serial.println("display: init");
    if (!board->init()) return "board init failed";
    if (!board->getLCD()) return "no LCD driver";
    if (!board->getLCD()->configFrameBufferNumber(2)) return "double buffering unavailable";
    Serial.println("display: begin");
    if (!board->begin()) return "board begin failed";

    auto lcd = board->getLCD();
    for (int i = 0; i < 2; i++) {
        buffers[i] = (uint16_t *)lcd->getFrameBufferByIndex(i);
        if (!buffers[i]) return "no frame buffer";
        memset(buffers[i], 0, SCREEN_WIDTH * SCREEN_HEIGHT * 2);
    }
    lcd->attachRefreshFinishCallback(onRefreshFinish);
    lcd->switchFrameBufferTo(buffers[front]);

    if (board->getBacklight()) board->getBacklight()->on();
    else Serial.println("display: no backlight driver");
    Serial.println("display: ok");
    return nullptr;
}

uint16_t *frameBuffer() {
    return buffers[1 - front];
}

void flushDisplay() {
    if (!board || !board->getLCD()) return;
    xSemaphoreTake(vsync, 0);
    xSemaphoreTake(vsync, pdMS_TO_TICKS(30));
    board->getLCD()->switchFrameBufferTo(buffers[1 - front]);
    front = 1 - front;
}

void setBrightness(int percent) {
    if (board && board->getBacklight()) board->getBacklight()->setBrightness(constrain(percent, 5, 100));
}

void setBacklight(bool on) {
    if (!board || !board->getBacklight()) return;
    if (on) board->getBacklight()->on();
    else board->getBacklight()->off();
}

void resyncDisplay() {
    if (board && board->getLCD()) esp_lcd_rgb_panel_restart(board->getLCD()->getRefreshPanelHandle());
}
