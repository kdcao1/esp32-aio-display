import os
from tkinter import colorchooser, filedialog, messagebox

import customtkinter as ctk

import config

CUSTOM = "Custom"
COLOR_MODE_LABELS = {"gradient": "Gradient", "temperature": "Follows temperature"}
QUALITY_LABELS = {"sharp": "Sharp (480)", "smooth": "Smooth (240, faster)"}
CYCLE_LABELS = {0: "Off", 30: "30 seconds", 60: "1 minute", 300: "5 minutes", 900: "15 minutes"}


def format_bytes(n):
    for unit, size in (("GB", 1e9), ("MB", 1e6), ("KB", 1e3)):
        if n >= size:
            return f"{n / size:.1f} {unit}"
    return f"{n} B"


class SettingsWindow(ctk.CTkToplevel):
    def __init__(self, master, app):
        super().__init__(master)
        self.app = app
        self.title("Cooler Screen")
        self.geometry("460x620")
        self.resizable(False, False)
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(2, weight=1)
        self.protocol("WM_DELETE_WINDOW", self.withdraw)

        ctk.CTkLabel(self, text="Cooler Screen", font=ctk.CTkFont(size=22, weight="bold")).grid(
            row=0, column=0, padx=20, pady=(20, 4), sticky="w")
        self.status = ctk.CTkLabel(self, text="", anchor="w", wraplength=400, justify="left")
        self.status.grid(row=1, column=0, padx=20, sticky="ew")

        tabs = ctk.CTkTabview(self)
        tabs.grid(row=2, column=0, padx=12, pady=(4, 12), sticky="nsew")
        self._build_display(tabs.add("Display"))
        self._build_gifs(tabs.add("GIF library"))

        self.selected = None
        self.gif_rows = {}
        self.refresh()
        self._poll_status()
        self.refresh_gifs()

    def _section(self, parent, text, row):
        ctk.CTkLabel(parent, text=text, font=ctk.CTkFont(size=14, weight="bold")).grid(
            row=row, column=0, padx=8, pady=(14, 6), sticky="w")

    def _build_display(self, tab):
        tab.grid_columnconfigure(0, weight=1)
        self._section(tab, "Mode", 0)
        labels = [config.MODE_LABELS[m] for m in config.MODES]
        self.mode = ctk.CTkSegmentedButton(tab, values=labels, command=self._on_mode)
        self.mode.grid(row=1, column=0, padx=8, sticky="ew")

        self._section(tab, "Ring colors", 2)
        row = ctk.CTkFrame(tab, fg_color="transparent")
        row.grid(row=3, column=0, padx=8, sticky="ew")
        self.theme = ctk.CTkOptionMenu(row, values=[*config.THEMES, CUSTOM], command=self._on_theme, width=150)
        self.theme.pack(side="left")
        self.swatch_end = ctk.CTkButton(row, text="", width=36, command=lambda: self._pick_color(1))
        self.swatch_end.pack(side="right")
        self.swatch_start = ctk.CTkButton(row, text="", width=36, command=lambda: self._pick_color(0))
        self.swatch_start.pack(side="right", padx=8)
        self.color_mode = ctk.CTkSegmentedButton(tab, values=list(COLOR_MODE_LABELS.values()),
                                                 command=self._on_color_mode)
        self.color_mode.grid(row=4, column=0, padx=8, pady=(10, 0), sticky="ew")

        self._section(tab, "Brightness", 5)
        self.brightness = ctk.CTkSlider(tab, from_=5, to=100, number_of_steps=19, command=self._on_brightness)
        self.brightness.grid(row=6, column=0, padx=8, sticky="ew")

        self._section(tab, "System", 7)
        self.autostart = ctk.CTkCheckBox(tab, text="Start with Windows", command=self._on_autostart)
        self.autostart.grid(row=8, column=0, padx=8, sticky="w")
        if self.app.autostart_enabled():
            self.autostart.select()

    def _build_gifs(self, tab):
        tab.grid_columnconfigure(0, weight=1)
        tab.grid_rowconfigure(1, weight=1)
        self.storage = ctk.CTkLabel(tab, text="Reading library...", anchor="w")
        self.storage.grid(row=0, column=0, padx=8, pady=(6, 4), sticky="ew")

        self.gif_list = ctk.CTkScrollableFrame(tab, height=170)
        self.gif_list.grid(row=1, column=0, padx=8, sticky="nsew")
        self.gif_list.grid_columnconfigure(0, weight=1)

        buttons = ctk.CTkFrame(tab, fg_color="transparent")
        buttons.grid(row=2, column=0, padx=8, pady=(8, 0), sticky="ew")
        self.upload_btn = ctk.CTkButton(buttons, text="Upload...", width=90, command=self._upload)
        self.upload_btn.pack(side="left")
        self.play_btn = ctk.CTkButton(buttons, text="Play", width=70, command=self._play)
        self.play_btn.pack(side="left", padx=6)
        self.delete_btn = ctk.CTkButton(buttons, text="Delete", width=70, fg_color="#8B2C2C",
                                        hover_color="#A33636", command=self._delete)
        self.delete_btn.pack(side="left")
        ctk.CTkButton(buttons, text="Refresh", width=70, command=self.refresh_gifs).pack(side="right")

        options = ctk.CTkFrame(tab, fg_color="transparent")
        options.grid(row=3, column=0, padx=8, pady=(10, 0), sticky="ew")
        options.grid_columnconfigure(1, weight=1)
        ctk.CTkLabel(options, text="Upload quality").grid(row=0, column=0, sticky="w")
        self.quality = ctk.CTkSegmentedButton(options, values=list(QUALITY_LABELS.values()), command=self._on_quality)
        self.quality.grid(row=0, column=1, padx=(10, 0), sticky="ew")
        ctk.CTkLabel(options, text="Cycle GIFs every").grid(row=1, column=0, pady=(8, 0), sticky="w")
        self.cycle = ctk.CTkOptionMenu(options, values=list(CYCLE_LABELS.values()), command=self._on_cycle)
        self.cycle.grid(row=1, column=1, padx=(10, 0), pady=(8, 0), sticky="w")

        self.progress = ctk.CTkProgressBar(tab)
        self.progress.set(0)
        self.progress.grid(row=4, column=0, padx=8, pady=(12, 0), sticky="ew")
        self.upload_status = ctk.CTkLabel(tab, text="GIFs are cropped to fit the round screen.", anchor="w",
                                          wraplength=400, justify="left")
        self.upload_status.grid(row=5, column=0, padx=8, sticky="ew")

    def refresh(self):
        cfg = self.app.cfg
        self.mode.set(config.MODE_LABELS[cfg["mode"]])
        self.theme.set(cfg["theme"] if cfg["theme"] in config.THEMES else CUSTOM)
        self.swatch_start.configure(fg_color=cfg["color_start"], hover_color=cfg["color_start"])
        self.swatch_end.configure(fg_color=cfg["color_end"], hover_color=cfg["color_end"])
        self.brightness.set(cfg["brightness"])
        self.color_mode.set(COLOR_MODE_LABELS[cfg["color_mode"]])
        self.quality.set(QUALITY_LABELS[cfg["gif_quality"]])
        self.cycle.set(CYCLE_LABELS.get(cfg["gif_cycle"], "Off"))

    def _poll_status(self):
        if not self.winfo_exists():
            return
        text, color = self.app.status_text()
        self.status.configure(text=text, text_color=color)
        self.after(1000, self._poll_status)

    def refresh_gifs(self):
        self.app.refresh_gifs(self._show_gifs)

    def _show_gifs(self, info):
        for widget in self.gif_list.winfo_children():
            widget.destroy()
        self.gif_rows = {}
        if info is None:
            self.storage.configure(text="Device not connected")
            return
        where = "SD card" if info["storage"] == "SD" else "Internal flash (no SD card found)"
        self.storage.configure(text=f"{where} - {format_bytes(info['free'])} free")
        if self.selected not in {name for name, _ in info["gifs"]}:
            self.selected = info["current"]
        for i, (name, size) in enumerate(info["gifs"]):
            playing = " (playing)" if name == info["current"] and self.app.cfg["mode"] == "GIF" else ""
            button = ctk.CTkButton(self.gif_list, text=f"{name}{playing}    {format_bytes(size)}", anchor="w",
                                   fg_color="transparent", text_color=("gray10", "gray90"),
                                   command=lambda n=name: self._select(n))
            button.grid(row=i, column=0, pady=1, sticky="ew")
            self.gif_rows[name] = button
        if not info["gifs"]:
            ctk.CTkLabel(self.gif_list, text="No GIFs yet - upload one.").grid(row=0, column=0, pady=10)
        self._highlight()

    def _select(self, name):
        self.selected = name
        self._highlight()

    def _highlight(self):
        for name, button in self.gif_rows.items():
            button.configure(fg_color=("#3B8ED0", "#1F6AA5") if name == self.selected else "transparent")
        state = "normal" if self.selected in self.gif_rows else "disabled"
        self.play_btn.configure(state=state)
        self.delete_btn.configure(state=state)

    def _play(self):
        if self.selected:
            self.app.play_gif(self.selected)
            self.refresh()
            self.after(300, self.refresh_gifs)

    def _delete(self):
        name = self.selected
        if name and messagebox.askyesno("Cooler Screen", f"Delete '{name}' from the screen?", parent=self):
            self.selected = None
            self.app.delete_gif(name, lambda _: self.refresh_gifs())

    def _on_quality(self, label):
        self.app.set_gif_quality(next(k for k, v in QUALITY_LABELS.items() if v == label))

    def _on_cycle(self, label):
        self.app.set_gif_cycle(next(k for k, v in CYCLE_LABELS.items() if v == label))

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
        self.refresh_gifs()
