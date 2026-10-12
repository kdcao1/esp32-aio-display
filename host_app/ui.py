import os
from tkinter import colorchooser, filedialog, messagebox

import customtkinter as ctk

import config

CUSTOM = "Custom"
COLOR_MODE_LABELS = {"gradient": "Gradient", "temperature": "Follows temperature"}


class SettingsWindow(ctk.CTkToplevel):
    def __init__(self, master, app):
        super().__init__(master)
        self.app = app
        self.title("Cooler Screen")
        self.geometry("440x680")
        self.resizable(False, False)
        self.grid_columnconfigure(0, weight=1)
        self.protocol("WM_DELETE_WINDOW", self.withdraw)

        ctk.CTkLabel(self, text="Cooler Screen", font=ctk.CTkFont(size=22, weight="bold")).grid(
            row=0, column=0, padx=20, pady=(20, 4), sticky="w")
        self.status = ctk.CTkLabel(self, text="", anchor="w", wraplength=380, justify="left")
        self.status.grid(row=1, column=0, padx=20, sticky="ew")

        self._section("Display", 2)
        labels = [config.MODE_LABELS[m] for m in config.MODES]
        self.mode = ctk.CTkSegmentedButton(self, values=labels, command=self._on_mode)
        self.mode.grid(row=3, column=0, padx=20, sticky="ew")

        self._section("Ring colors", 4)
        row = ctk.CTkFrame(self, fg_color="transparent")
        row.grid(row=5, column=0, padx=20, sticky="ew")
        self.theme = ctk.CTkOptionMenu(row, values=[*config.THEMES, CUSTOM], command=self._on_theme, width=150)
        self.theme.pack(side="left")
        self.swatch_end = ctk.CTkButton(row, text="", width=36, command=lambda: self._pick_color(1))
        self.swatch_end.pack(side="right")
        self.swatch_start = ctk.CTkButton(row, text="", width=36, command=lambda: self._pick_color(0))
        self.swatch_start.pack(side="right", padx=8)
        self.color_mode = ctk.CTkSegmentedButton(self, values=list(COLOR_MODE_LABELS.values()),
                                                 command=self._on_color_mode)
        self.color_mode.grid(row=6, column=0, padx=20, pady=(10, 0), sticky="ew")

        self._section("Brightness", 7)
        self.brightness = ctk.CTkSlider(self, from_=5, to=100, number_of_steps=19, command=self._on_brightness)
        self.brightness.grid(row=8, column=0, padx=20, sticky="ew")

        self._section("Animated GIF", 9)
        self.upload_btn = ctk.CTkButton(self, text="Upload GIF...", command=self._upload)
        self.upload_btn.grid(row=10, column=0, padx=20, sticky="w")
        self.progress = ctk.CTkProgressBar(self)
        self.progress.set(0)
        self.progress.grid(row=11, column=0, padx=20, pady=(10, 0), sticky="ew")
        self.upload_status = ctk.CTkLabel(self, text="GIFs are cropped to fit the round screen.", anchor="w",
                                          wraplength=400, justify="left")
        self.upload_status.grid(row=12, column=0, padx=20, sticky="ew")

        self._section("System", 13)
        self.autostart = ctk.CTkCheckBox(self, text="Start with Windows", command=self._on_autostart)
        self.autostart.grid(row=14, column=0, padx=20, sticky="w")
        if self.app.autostart_enabled():
            self.autostart.select()

        self.refresh()
        self._poll_status()

    def _section(self, text, row):
        ctk.CTkLabel(self, text=text, font=ctk.CTkFont(size=14, weight="bold")).grid(
            row=row, column=0, padx=20, pady=(18, 6), sticky="w")

    def refresh(self):
        cfg = self.app.cfg
        self.mode.set(config.MODE_LABELS[cfg["mode"]])
        self.theme.set(cfg["theme"] if cfg["theme"] in config.THEMES else CUSTOM)
        self.swatch_start.configure(fg_color=cfg["color_start"], hover_color=cfg["color_start"])
        self.swatch_end.configure(fg_color=cfg["color_end"], hover_color=cfg["color_end"])
        self.brightness.set(cfg["brightness"])
        self.color_mode.set(COLOR_MODE_LABELS[cfg["color_mode"]])

    def _poll_status(self):
        if not self.winfo_exists():
            return
        text, color = self.app.status_text()
        self.status.configure(text=text, text_color=color)
        self.after(1000, self._poll_status)

    def _on_mode(self, label):
        mode = next(m for m, l in config.MODE_LABELS.items() if l == label)
        self.app.set_mode(mode)

    def _on_theme(self, name):
        if name in config.THEMES:
            self.app.set_theme(name, *config.THEMES[name])
        self.refresh()

    def _pick_color(self, index):
        key = ("color_start", "color_end")[index]
        _, hex_color = colorchooser.askcolor(color=self.app.cfg[key], parent=self)
        if not hex_color:
            return
        colors = [self.app.cfg["color_start"], self.app.cfg["color_end"]]
        colors[index] = hex_color.upper()
        self.app.set_theme(CUSTOM, *colors)
        self.refresh()

    def _on_color_mode(self, label):
        self.app.set_color_mode(next(k for k, v in COLOR_MODE_LABELS.items() if v == label))

    def _on_autostart(self):
        error = self.app.set_autostart(bool(self.autostart.get()))
        if error:
            self.autostart.toggle()
            messagebox.showerror("Cooler Screen", f"Couldn't change Start with Windows:\n{error}", parent=self)

    def _on_brightness(self, value):
        self.app.set_brightness(value)

    def _upload(self):
        path = filedialog.askopenfilename(parent=self, title="Select a GIF", filetypes=[("GIF files", "*.gif")])
        if not path:
            return
        self.upload_btn.configure(state="disabled")
        self.progress.set(0)
        self.upload_status.configure(text=f"Preparing {os.path.basename(path)}...", text_color="orange")
        self.app.upload_gif(path, self._on_progress, self._on_upload_done)

    def _on_progress(self, fraction, message=None):
        self.progress.set(fraction)
        self.upload_status.configure(text=message or f"Uploading... {fraction * 100:.0f}%")

    def _on_upload_done(self, error):
        self.upload_btn.configure(state="normal")
        if error:
            self.upload_status.configure(text=f"Upload failed: {error}", text_color="red")
            return
        self.progress.set(1)
        self.upload_status.configure(text="Upload complete", text_color="green")
        self.app.set_mode("GIF")
