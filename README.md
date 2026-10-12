# Cooler Screen

An NZXT Kraken-style LCD for a Waveshare ESP32-S3-LCD-2.8C (480x480 round, no-touch variant), driven by CPU/GPU stats from a Windows tray app.

![Preview](docs/preview.png)

Modes: **CPU**, **GPU**, **CPU + GPU**, **Liquid** (coolant temp, pump and fan RPM) gauges, or a looping **GIF**. Ring colors (gradient or color that follows temperature), brightness and mode are set from the tray app and remembered by the device.

## Firmware (`esp32_app`)

Requires [PlatformIO](https://platformio.org/). The project uses the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform (Arduino core 3.x).

```
cd esp32_app
pio run -t upload
```

The board connects over its native USB port (USB CDC). Board pins and timing live in `include/esp_panel_board_custom_conf.h` (touch disabled, pixel clock raised to 26 MHz to remove flicker on dim colors). If upload fails, hold **BOOT**, tap **RESET**, release **BOOT** and retry.

To preview the dashboard on a desktop: `g++ -O2 -std=c++17 -Isrc tools/preview.cpp src/gfx.cpp src/dashboard.cpp -o preview && ./preview .`
Fonts are generated from Montserrat (SIL OFL) with `python tools/gen_fonts.py` (needs Pillow).

## PC app (`host_app`, Windows)

```
cd host_app
py -3.13 -m venv venv
venv\Scripts\pip install -r requirements.txt
venv\Scripts\pythonw tray_app.py
```

To build a standalone app instead, run `build_exe.bat`; it produces `dist\CoolerScreen\CoolerScreen.exe` (no Python needed on the target PC, copy the whole `CoolerScreen` folder). Tick **Start with Windows** in Settings to launch it at logon via Task Scheduler with admin rights, without a UAC prompt.

The app asks for administrator rights (needed by LibreHardwareMonitor for CPU temperatures); pass `--no-elevate` to skip. Settings are stored in `%APPDATA%\CoolerScreen\config.json`; set `"port"` there to pin a COM port instead of auto-detecting.

GIFs live in a library on the microSD card (falls back to internal flash without a card) and are managed from the **GIF library** tab: upload, play, delete, and cycle through them on a timer. Uploads are cropped to 480x480 (**Sharp**) or 240x240 scaled 2x (**Smooth**, about 4x faster to decode), capped at 300 frames and 5 MB each.

Liquid temperature, pump and fan RPM are detected automatically by sensor name. If the wrong sensor is picked (or none), run `venv\Scripts\python monitor.py --list` as administrator and put part of the right sensor's name into `liquid_sensor`, `pump_sensor` or `fan_sensor` in the config file.

## Serial protocol

Newline-terminated commands:

| Command | Description |
| --- | --- |
| `PING` | Replies `PONG COOLERSCREEN 5` |
| `STAT:<cpuT>,<cpuLoad>,<gpuT>,<gpuLoad>,<liquidT>,<pumpRpm>,<fanRpm>` | Empty field = unavailable |
| `MODE:CPU\|GPU\|DUAL\|LIQUID\|GIF` | |
| `COLORMODE:GRADIENT\|TEMP` | Gradient along the ring, or one color picked by temperature |
| `THEME:<RRGGBB>,<RRGGBB>` | Ring gradient |
| `BRIGHT:<5-100>` | |
| `TRACK:<RRGGBB>` | Gauge track color |
| `UPLOAD:<size>,<crc32 hex>,<name>` | Device replies `READY`. Each 4096-byte chunk is followed by its CRC32 (4 bytes, little-endian); the device replies `ACK <n>` or `NAK <n> <reason>` (resend the chunk). The file is buffered in PSRAM and saved as `/gifs/<name>.gif` at the end, then `OK <resends>` or `ERR <reason>` |
| `GIFLIST` | `GIF <name> <bytes>` per file, then `END <free bytes> SD\|FLASH <current\|-> <cycle seconds>` |
| `GIFPLAY:<name>` | Switch to GIF mode playing that file |
| `GIFDEL:<name>` | Delete a GIF; replies `DONE` |
| `GIFCYCLE:<seconds>` | Rotate through the library; `0` turns it off |
