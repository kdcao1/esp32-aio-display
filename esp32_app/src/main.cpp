#include <Arduino.h>
#include <LittleFS.h>
#include <AnimatedGIF.h>
#include "display.h"

enum Mode { MODE_TEMP, MODE_GIF };
Mode currentMode = MODE_TEMP;

AnimatedGIF gif;
File gifFile;
String currentTemp = "--";

// GIF callback functions
void * GIFOpenFile(const char *fname, int32_t *pSize) {
    gifFile = LittleFS.open(fname, "r");
    if (gifFile) {
        *pSize = gifFile.size();
        return (void *)&gifFile;
    }
    return NULL;
}

void GIFCloseFile(void *pHandle) {
    File *f = static_cast<File *>(pHandle);
    if (f != NULL) f->close();
}

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
    int32_t iBytesRead;
    iBytesRead = iLen;
    File *f = static_cast<File *>(pFile->fHandle);
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos - 1;
    if (iBytesRead <= 0) return 0;
    iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
    pFile->iPos = f->position();
    return iBytesRead;
}

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
    int i = micros();
    File *f = static_cast<File *>(pFile->fHandle);
    f->seek(iPosition);
    pFile->iPos = (int32_t)f->position();
    i = micros() - i;
    return iPosition;
}

void GIFDraw(GIFDRAW *pDraw) {
    uint8_t *s;
    uint16_t *d, *usPalette, usTemp[320];
    int x, y, iWidth;

    iWidth = pDraw->iWidth;
    if (iWidth > SCREEN_WIDTH) iWidth = SCREEN_WIDTH;
    
    usPalette = pDraw->pPalette;
    y = pDraw->iY + pDraw->y; // current line
    
    if (y >= SCREEN_HEIGHT || pDraw->iX >= SCREEN_WIDTH || iWidth < 1) return;

    s = pDraw->pPixels;
    if (pDraw->ucHasTransparency) {
        // Handle transparency
        uint8_t c, ucTransparent = pDraw->ucTransparent;
        int x = 0;
        // In a real optimized system we would read the current fb line, but for simplicity we skip
        // For now, this requires a background color fill before GIF loop.
    } else {
        s = pDraw->pPixels;
        // Translate 8-bit pixels through RGB565 palette
        for (x=0; x<iWidth; x++) usTemp[x] = usPalette[*s++];
        pushPixels(pDraw->iX, y, iWidth, 1, usTemp);
    }
}

void processSerial() {
    if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        
        if (cmd.startsWith("MODE:")) {
            String modeStr = cmd.substring(5);
            if (modeStr == "TEMP") currentMode = MODE_TEMP;
            else if (modeStr == "GIF") {
                currentMode = MODE_GIF;
                fillScreen(0x0000);
            }
        } 
        else if (cmd.startsWith("TEMP:")) {
            currentTemp = cmd.substring(5);
        }
        else if (cmd.startsWith("UPLOAD:")) {
            int fileSize = cmd.substring(7).toInt();
            Serial.printf("Ready for upload: %d bytes\n", fileSize);
            // ACK 
            // Read binary data
            File f = LittleFS.open("/anim.gif", "w");
            if (f) {
                int bytesRead = 0;
                uint8_t buf[1024];
                while(bytesRead < fileSize) {
                    if (Serial.available()) {
                        int toRead = Serial.readBytes(buf, min((int)sizeof(buf), fileSize - bytesRead));
                        f.write(buf, toRead);
                        bytesRead += toRead;
                    }
                    delay(1);
                }
                f.close();
                Serial.println("Upload complete");
            }
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Init LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }

    initDisplay();
    
    gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop() {
    processSerial();
    
    if (currentMode == MODE_TEMP) {
        fillScreen(0x0000); // clear screen
        // Basic drawing, normally use proper fonts here
        drawText(currentTemp.c_str(), 100, 200, 0xFFFF);
        delay(100);
    } 
    else if (currentMode == MODE_GIF) {
        if (gif.open("/anim.gif", GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
            while (gif.playFrame(true, NULL)) {
                processSerial();
                if (currentMode != MODE_GIF) break;
            }
            gif.close();
        } else {
            // No gif found
            delay(100);
        }
    }
}
