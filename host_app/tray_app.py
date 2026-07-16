import threading
import time
import pystray
from PIL import Image, ImageDraw
import sys
import os

from comms import ESP32Comms
from monitor import init_monitor, get_cpu_temp
from ui import show_settings

# Global state
comms = None
monitor = None
tray_icon = None
running = True
current_mode = "TEMP"

def create_icon():
    # Create a simple icon (e.g. a colored square or a cool icon)
    image = Image.new('RGB', (64, 64), color=(30, 30, 30))
    draw = ImageDraw.Draw(image)
    draw.rectangle([16, 16, 48, 48], fill=(0, 150, 255))
    return image

def temperature_loop():
    global running, current_mode, comms, monitor
    
    # Try to initialize monitor
    try:
        monitor = init_monitor()
    except Exception as e:
        print(f"Monitor init failed: {e}")
        
    while running:
        if monitor and comms.connected and current_mode == "TEMP":
            temp = get_cpu_temp(monitor)
            if temp is not None:
                comms.update_temp(temp)
        time.sleep(2) # Update every 2 seconds

def on_open_settings(icon, item):
    # Running UI in the main thread or a separate thread might be tricky with tkinter.
    # Usually UI needs to run in the main thread.
    # Since pystray's icon.run() blocks the main thread, we should launch UI in a separate thread.
    # customtkinter can handle running in a separate thread if initialized carefully, 
    # but it's better to run pystray in a separate thread and UI in the main thread when needed,
    # or just use multiprocessing.
    
    # For simplicity, we just spawn a thread for the UI
    ui_thread = threading.Thread(target=show_settings, args=(comms,), daemon=True)
    ui_thread.start()

def on_exit(icon, item):
    global running
    running = False
    if comms:
        comms.disconnect()
    icon.stop()

def main():
    global comms, tray_icon

    comms = ESP32Comms()
    comms.connect()

    # Start temperature polling thread
    temp_thread = threading.Thread(target=temperature_loop, daemon=True)
    temp_thread.start()

    # Setup tray icon
    menu = pystray.Menu(
        pystray.MenuItem("Settings", on_open_settings, default=True),
        pystray.MenuItem("Exit", on_exit)
    )
    
    tray_icon = pystray.Icon("CoolerScreen", create_icon(), "Cooler Screen", menu)
    
    print("Starting tray application...")
    tray_icon.run()

if __name__ == "__main__":
    main()
