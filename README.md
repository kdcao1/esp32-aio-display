# Cooler Screen

An NZXT Kraken-style LCD for a Waveshare ESP32-S3-LCD-2.8C (480x480 round, no-touch variant), driven by CPU/GPU stats from a Windows tray app.

![Preview](docs/preview.png)

Modes: **CPU**, **GPU**, **CPU + GPU**, **Liquid** (coolant temp, pump and fan RPM) gauges, or a looping **GIF**. Ring colors (gradient or color that follows temperature), brightness and mode are set from the tray app and remembered by the device.

## Download

[Releases](../../releases) have a ready-to-run Windows app (`CoolerScreen-windows.zip`) plus firmware. Flash `firmware-full.bin` once with `esptool.py --chip esp32s3 write_flash 0x0 firmware-full.bin` (or build it below); after that the app's **Device** tab updates it. A 3D-printable tape-on mount is in [`enclosure`](enclosure).

Releases are built by GitHub Actions when a `v*` tag is pushed (`git tag v1.0 && git push origin v1.0`).

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

Sensors come from LibreHardwareMonitor 0.9.6 (`host_app/lhm`), which needs the [PawnIO](https://pawnio.eu) driver installed (FanControl and recent LibreHardwareMonitor installs already include it). The app asks for administrator rights, which Windows requires for CPU temperatures; pass `--no-elevate` to skip. Settings are stored in `%APPDATA%\CoolerScreen\config.json`; set `"port"` there to pin a COM port instead of auto-detecting.

GIFs live in a library on the microSD card (falls back to internal flash without a card) and are managed from the **GIF library** tab: upload, play, delete, and cycle through them on a timer. Every upload stores two copies: 480x480 (**Sharp**) and 240x240 scaled 2x (**Smooth**, about 4x faster to decode); **Playback quality** switches between them instantly. Copies are capped at 300 frames and 5 MB (a GIF too large for Sharp keeps only its Smooth copy).

Sensors are picked automatically; the **Sensors** tab lets you choose the exact sensor for CPU/GPU/liquid temperature and pump/fan speed (`venv\Scripts\python monitor.py --list` prints everything available).

The **Device** tab sets rotation, screen power (off when the PC is idle, locked, or asleep/shut down; the board blanks itself after 60 s without the PC) and updates firmware: `build_exe.bat` bundles the last `pio run` build, and the app offers **Update firmware** when the board runs something different. **Flash file...** flashes any `firmware.bin`.

## Serial protocol

Newline-terminated commands:

| Command | Description |
| --- | --- |
| `PING` | Replies `PONG COOLERSCREEN 6` |
| `STAT:<cpuT>,<cpuLoad>,<gpuT>,<gpuLoad>,<liquidT>,<pumpRpm>,<fanRpm>` | Empty field = unavailable |
| `MODE:CPU\|GPU\|DUAL\|LIQUID\|GIF` | |
| `COLORMODE:GRADIENT\|TEMP` | Gradient along the ring, or one color picked by temperature |
| `THEME:<RRGGBB>,<RRGGBB>` | Ring gradient |
| `BRIGHT:<5-100>` | |
| `TRACK:<RRGGBB>` | Gauge track color |
| `UPLOAD:<size>,<crc32 hex>,<name>,H\|S` | Device replies `READY`. Each 4096-byte chunk is followed by its CRC32 (4 bytes, little-endian); the device replies `ACK <n>` or `NAK <n> <reason>` (resend the chunk). The file is buffered in PSRAM and saved as `/gifs/<name>.gif` (H, sharp) or `/gifs240/<name>.gif` (S, smooth) at the end, then `OK <resends>` or `ERR <reason>` |
| `GIFLIST` | `GIF <name> <bytes> <H\|S\|HS>` per GIF, then `END <free bytes> SD\|FLASH <current\|-> <cycle seconds>` |
| `GIFPLAY:<name>` | Switch to GIF mode playing that file |
| `GIFDEL:<name>` | Delete a GIF; replies `DONE` |
| `QUALITY:SHARP\|SMOOTH` | Which copy to play |
| `GIFCYCLE:<seconds>` | Rotate through the library; `0` turns it off |
| `ROTATE:0\|90\|180\|270` | Screen rotation |
| `SCREEN:ON\|OFF` | Backlight on/off |
| `SLEEP:<seconds>` | Blank the screen after this long without any command (`0` = never) |
| `VERSION` | `VERSION <app ELF SHA-256 prefix> <protocol>` |

## License

MIT, see [LICENSE](LICENSE). Bundled and linked third-party components are listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
