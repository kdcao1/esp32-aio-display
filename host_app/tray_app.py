import ctypes
import os
import queue
import sys
import threading
import time

import customtkinter as ctk
import pystray
from PIL import Image, ImageDraw

import config
from comms import ESP32Comms, UploadError
from gif_tools import prepare_gif
from monitor import HardwareMonitor, is_admin
from ui import SettingsWindow


def relaunch_as_admin():
    if os.name != "nt" or is_admin() or "--no-elevate" in sys.argv:
        return False
    params = " ".join(f'"{a}"' for a in [os.path.abspath(sys.argv[0]), *sys.argv[1:], "--no-elevate"])
    return ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, params, None, 1) > 32


def create_icon():
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([2, 2, 62, 62], fill=(10, 10, 14, 255))
    draw.arc([8, 8, 56, 56], 130, 410, fill=(40, 40, 50, 255), width=7)
    draw.arc([8, 8, 56, 56], 130, 320, fill=(45, 124, 255, 255), width=7)
    return image


class App:
    def __init__(self):
        self.cfg = config.load()
        self.comms = ESP32Comms(port=self.cfg["port"])
        self.monitor = None
        self.monitor_error = None
        self.stats = {}
        self.running = True
        self.tasks = queue.Queue()
        self.settings = None

        ctk.set_appearance_mode("dark")
        ctk.set_default_color_theme("blue")
        self.root = ctk.CTk()
        self.root.withdraw()

        menu = pystray.Menu(
            pystray.MenuItem("Settings", lambda: self.call_soon(self.open_settings), default=True),
            pystray.Menu.SEPARATOR,
            *[pystray.MenuItem(config.MODE_LABELS[m], self._mode_action(m), checked=self._mode_checked(m), radio=True)
              for m in config.MODES],
            pystray.Menu.SEPARATOR,
            pystray.MenuItem("Exit", lambda: self.call_soon(self.exit)),
        )
        self.icon = pystray.Icon("CoolerScreen", create_icon(), "Cooler Screen", menu)

    def _mode_action(self, mode):
        return lambda: self.call_soon(lambda: self.set_mode(mode))

    def _mode_checked(self, mode):
        return lambda item: self.cfg["mode"] == mode

    def call_soon(self, fn):
        self.tasks.put(fn)

    def _drain_tasks(self):
        while not self.tasks.empty():
            self.tasks.get_nowait()()
        if self.running:
            self.root.after(100, self._drain_tasks)

    def save(self):
        config.save(self.cfg)

    def set_mode(self, mode):
        self.cfg["mode"] = mode
        self.save()
        self.comms.set_mode(mode)
        self.icon.update_menu()
        if self.settings:
            self.settings.refresh()

    def set_theme(self, name, c0, c1):
        self.cfg.update(theme=name, color_start=c0, color_end=c1)
        self.save()
        self.comms.set_theme(c0, c1)

    def set_brightness(self, value):
        self.cfg["brightness"] = int(value)
        self.save()
        self.comms.set_brightness(value)

    def push_settings(self):
        self.comms.set_theme(self.cfg["color_start"], self.cfg["color_end"])
        self.comms.set_brightness(self.cfg["brightness"])
        self.comms.set_mode(self.cfg["mode"])

    def upload_gif(self, path, on_progress, on_done):
        def work():
            try:
                data = prepare_gif(path)
                self.comms.upload_gif(data, lambda p: self.call_soon(lambda: on_progress(p)))
                self.call_soon(lambda: on_done(None))
            except (UploadError, OSError, ValueError) as e:
                msg = str(e)
                if msg.startswith("ERR SIZE"):
                    limit = int(msg.split()[-1]) / 1_000_000
                    msg = f"GIF too large ({len(data) / 1_000_000:.1f} MB, max {limit:.1f} MB)"
                self.call_soon(lambda: on_done(msg))

        threading.Thread(target=work, daemon=True).start()

    def status_text(self):
        if not self.comms.connected:
            return "Searching for device...", "orange"
        if self.monitor_error:
            return f"Connected ({self.comms.device}) - sensors unavailable: {self.monitor_error}", "orange"
        if self.stats.get("cpu_temp") is None:
            hint = "" if is_admin() else " (run as administrator)"
            return f"Connected ({self.comms.device}) - no CPU temperature{hint}", "orange"
        return f"Connected ({self.comms.device})", "green"

    def worker(self):
        try:
            self.monitor = HardwareMonitor()
        except Exception as e:
            self.monitor_error = str(e)
            print(f"Monitor init failed: {e}")

        was_connected = False
        while self.running:
            started = time.time()
            if self.comms.ensure_connected():
                if not was_connected:
                    self.push_settings()
                if self.monitor:
                    try:
                        self.stats = self.monitor.read()
                    except Exception as e:
                        print(f"Sensor read failed: {e}")
                    self.comms.send_stats(self.stats.get("cpu_temp"), self.stats.get("cpu_load"),
                                          self.stats.get("gpu_temp"), self.stats.get("gpu_load"))
            was_connected = self.comms.connected
            self.icon.title = f"Cooler Screen - {self.status_text()[0]}"
            time.sleep(max(0.1, self.cfg["interval"] - (time.time() - started)))

    def open_settings(self):
        if self.settings and self.settings.winfo_exists():
            self.settings.deiconify()
            self.settings.focus_force()
            return
        self.settings = SettingsWindow(self.root, self)

    def exit(self):
        self.running = False
        self.icon.stop()
        self.comms.disconnect()
        if self.monitor:
            self.monitor.close()
        self.root.quit()

    def run(self):
        self.icon.run_detached()
        threading.Thread(target=self.worker, daemon=True).start()
        self.root.after(100, self._drain_tasks)
        self.root.mainloop()


if __name__ == "__main__":
    if relaunch_as_admin():
        sys.exit(0)
    App().run()
