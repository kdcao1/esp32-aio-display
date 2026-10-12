@echo off
setlocal
cd /d "%~dp0"
taskkill /IM CoolerScreen.exe /F >nul 2>&1
if exist dist\CoolerScreen rmdir /s /q dist\CoolerScreen 2>nul
if exist dist\CoolerScreen (
  echo Close Cooler Screen first ^(tray icon ^> Exit^), or run this from an admin terminal.
  exit /b 1
)
if not exist venv py -3.13 -m venv venv || exit /b 1
venv\Scripts\python -m pip install -r requirements.txt pyinstaller || exit /b 1
venv\Scripts\python icon.py icon.ico || exit /b 1
if exist firmware rmdir /s /q firmware
mkdir firmware
type nul > firmware\.keep
if exist ..\esp32_app\.pio\build\waveshare_2_8c\firmware.bin (
  copy /y ..\esp32_app\.pio\build\waveshare_2_8c\firmware.bin firmware\firmware.bin >nul
) else (
  echo Note: no firmware build found; run "pio run" in esp32_app to bundle firmware updates.
)
venv\Scripts\pyinstaller --noconfirm --clean --windowed --uac-admin --name CoolerScreen --icon icon.ico ^
  --add-data "lhm;lhm" --add-data "firmware;firmware" --collect-all esptool ^
  --collect-all customtkinter --collect-all clr_loader --collect-all pythonnet ^
  --hidden-import pystray._win32 tray_app.py || exit /b 1
echo.
echo Built dist\CoolerScreen\CoolerScreen.exe
