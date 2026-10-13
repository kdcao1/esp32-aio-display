# Third-party notices

Cooler Screen's own code is MIT licensed (see `LICENSE`). It includes or links the following.

## Bundled in this repository

| Component | License | Location |
| --- | --- | --- |
| [LibreHardwareMonitorLib](https://github.com/LibreHardwareMonitor/LibreHardwareMonitor) 0.9.6 | MPL-2.0 | `host_app/lhm`, license in `host_app/lhm/licenses/MPL-2.0.txt` |
| [DiskInfoToolkit](https://github.com/blacktempel/DiskInfoToolkit), [RAMSPDToolkit](https://github.com/blacktempel/RAMSPDToolkit) | MPL-2.0 | `host_app/lhm`, `host_app/lhm/licenses/MPL-2.0.txt` |
| [BlackSharp](https://github.com/blacktempel/BlackSharp) | MPL-2.0 | `host_app/lhm`, `host_app/lhm/licenses/BlackSharp.txt` |
| [HidSharp](https://github.com/IntergatedCircuits/HidSharp) | Apache-2.0 | `host_app/lhm`, `host_app/lhm/licenses/HidSharp.txt` |
| System.Buffers, System.Memory, System.Numerics.Vectors, System.Runtime.CompilerServices.Unsafe | MIT (.NET Foundation) | `host_app/lhm` |
| [Montserrat](https://github.com/JulietaUla/Montserrat) | SIL OFL 1.1 | `esp32_app/tools/fonts`, rasterized into `esp32_app/src/fonts` |

The .NET libraries are unmodified binaries; their source is available at the links above.

## Linked into the firmware

| Component | License |
| --- | --- |
| [Arduino core for ESP32](https://github.com/espressif/arduino-esp32) / ESP-IDF | LGPL-2.1 / Apache-2.0 |
| [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel), [ESP32_IO_Expander](https://github.com/esp-arduino-libs/ESP32_IO_Expander), [esp-lib-utils](https://github.com/esp-arduino-libs/esp-lib-utils) | Apache-2.0 |
| [AnimatedGIF](https://github.com/bitbank2/AnimatedGIF) | Apache-2.0 |

## Used by the PC app (installed from PyPI)

pyserial (BSD-3-Clause), pystray (LGPL-3.0), Pillow (MIT-CMU), customtkinter (MIT), pythonnet (MIT), esptool (GPL-2.0-or-later, used for firmware updates).

The Windows build (`CoolerScreen.exe`) bundles these packages, including esptool, so the build as a whole is distributed under the GPL-2.0-or-later; its complete source is this repository.
