import ctypes
import os
import queue
import sys
import threading
import time

import customtkinter as ctk
import pystray

import autostart
import config
from comms import ESP32Comms, UploadError
from gif_tools import FULL_SIZE, HALF_SIZE, gif_name, prepare_gif
from icon import create_icon
from monitor import HardwareMonitor, is_admin
from ui import SettingsWindow


def relaunch_as_admin():
    if os.name != "nt" or is_admin() or "--no-elevate" in sys.argv:
        return False
    args = [*sys.argv[1:], "--no-elevate"]
    if not getattr(sys, "frozen", False):
        args.insert(0, os.path.abspath(sys.argv[0]))
    params = " ".join(f'"{a}"' for a in args)
    return ctypes.windll.shell32.ShellExecuteW(None, "runas", sys.executable, params, None, 1) > 32


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

    def set_color_mode(self, value):
        self.cfg["color_mode"] = value
        self.save()
        self.comms.set_color_mode(config.COLOR_MODES[value])

    def autostart_enabled(self):
        try:
            return autostart.is_enabled()
        except OSError:
            return False

    def set_autostart(self, enabled):
        try:
            autostart.enable() if enabled else autostart.disable()
            return None
        except OSError as e:
            return str(e)

    def push_settings(self):
        self.comms.set_theme(self.cfg["color_start"], self.cfg["color_end"])
        self.comms.set_color_mode(config.COLOR_MODES[self.cfg["color_mode"]])
        self.comms.set_track(self.cfg["track_color"])
        self.comms.set_gif_cycle(self.cfg["gif_cycle"])
        self.comms.set_brightness(self.cfg["brightness"])
        self.comms.set_mode(self.cfg["mode"])

    def run_async(self, work, on_done=None):
        def runner():
            result = work()
            if on_done:
                self.call_soon(lambda: on_done(result))

        threading.Thread(target=runner, daemon=True).start()

    def refresh_gifs(self, on_done):
        self.run_async(self.comms.gif_list, on_done)

    def play_gif(self, name):
        self.cfg["mode"] = "GIF"
        self.save()
        self.comms.gif_play(name)
        self.icon.update_menu()

    def format_sd(self, on_done):
        self.run_async(self.comms.sd_format, on_done)

    def delete_gif(self, name, on_done):
        self.run_async(lambda: self.comms.gif_delete(name), on_done)

    def set_gif_cycle(self, seconds):
        self.cfg["gif_cycle"] = seconds
        self.save()
        self.comms.set_gif_cycle(seconds)

    def set_gif_quality(self, value):
        self.cfg["gif_quality"] = value
        self.save()

    def upload_gif(self, path, on_progress, on_done):
        name = gif_name(path)

        def report(fraction, message=None):
            self.call_soon(lambda: on_progress(fraction, message))

        def send(size):
            report(0, f"Preparing {size}x{size} GIF...")
            data = prepare_gif(path, size)
            report(0, f"Uploading {len(data) / 1_000_000:.1f} MB...")
            self.comms.upload_gif(data, name, report)

        def work():
            try:
                if self.cfg["gif_quality"] == "smooth":
                    send(HALF_SIZE)
                else:
                    try:
                        send(FULL_SIZE)
                    except UploadError as e:
                        if not str(e).startswith("ERR SIZE"):
                            raise
                        send(HALF_SIZE)
                self.cfg["mode"] = "GIF"
                self.save()
                return None
            except (UploadError, OSError, ValueError) as e:
                msg = str(e)
                if msg.startswith("ERR SIZE"):
                    msg = f"GIF too large (room for {int(msg.split()[-1]) / 1e6:.1f} MB)"
                return msg

        self.run_async(work, on_done)

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
            self.monitor = HardwareMonitor({k: self.cfg[k] for k in ("liquid_sensor", "pump_sensor", "fan_sensor")})
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
                    self.comms.send_stats(self.stats)
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
