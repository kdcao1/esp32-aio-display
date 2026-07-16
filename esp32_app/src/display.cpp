#include "display.h"
#include <ESP_Panel_Library.h>

ESP_Panel *panel = nullptr;

void initDisplay() {
    // Note: The ESP32_Display_Panel library supports ST7701
    // You may need to provide a custom esp_panel_board_custom_conf.h 
    // in the include directory for the specific 2.8C pins if defaults don't match.
    // For standard Waveshare 2.8C, make sure your board configuration matches.
    
    panel = new ESP_Panel();
    
    // Initialize the panel
    panel->init();
    panel->begin();
    
    // Turn on backlight
    panel->getBacklight()->on();
    
    fillScreen(0x0000); // Black
}

void fillScreen(uint16_t color) {
    // Fill the whole screen with a color (naive implementation)
    // In a real application, you'd use the 2D DMA or draw directly to the framebuffer.
    // For ESP_Panel, we get the framebuffer
    uint16_t *fb = (uint16_t *)panel->getLcd()->getColorBuffer();
    if (fb) {
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            fb[i] = color;
        }
        panel->getLcd()->drawBitmap(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, fb);
    }
}

void drawText(const char* text, int x, int y, uint16_t color) {
    // A full text rendering engine requires fonts (e.g. via TFT_eSPI or LVGL).
    // To keep it lightweight and focused on GIF/Temp, if LVGL isn't used,
    // we would include a font file. For the sake of this scaffolding,
    // we'll leave it as a placeholder. In production, LovyanGFX or LVGL is recommended for text.
    // Assuming we use a minimal approach or just draw simple lines for now:
    Serial.printf("Drawing text '%s' at %d,%d\n", text, x, y);
}

void pushPixels(int x, int y, int w, int h, uint16_t* pixels) {
    if (panel) {
        panel->getLcd()->drawBitmap(x, y, w, h, pixels);
    }
}
