@echo off
setlocal
cd /d "%~dp0"
if not exist venv py -3.13 -m venv venv || exit /b 1
venv\Scripts\python -m pip install -r requirements.txt pyinstaller || exit /b 1
venv\Scripts\python icon.py icon.ico || exit /b 1
venv\Scripts\pyinstaller --noconfirm --clean --windowed --uac-admin --name CoolerScreen --icon icon.ico ^
  --add-data "LibreHardwareMonitorLib.dll;." --add-data "HidSharp.dll;." ^
  --collect-all customtkinter --collect-all clr_loader --collect-all pythonnet ^
  --hidden-import pystray._win32 tray_app.py || exit /b 1
echo.
echo Built dist\CoolerScreen\CoolerScreen.exe
