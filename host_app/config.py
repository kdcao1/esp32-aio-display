import json
import os

THEMES = {
    "Kraken": ("#2D7CFF", "#B22CFF"),
    "Heat": ("#00D0FF", "#FF3B30"),
    "Mint": ("#00FFA3", "#00B4FF"),
    "Sunset": ("#FFB000", "#FF2D6F"),
    "Mono": ("#E6E6EB", "#E6E6EB"),
}

MODES = ("CPU", "GPU", "DUAL", "LIQUID", "GIF")
MODE_LABELS = {"CPU": "CPU", "GPU": "GPU", "DUAL": "CPU + GPU", "LIQUID": "Liquid", "GIF": "GIF"}
COLOR_MODES = {"gradient": "GRADIENT", "temperature": "TEMP"}

DEFAULTS = {
    "mode": "CPU",
    "theme": "Kraken",
    "color_start": THEMES["Kraken"][0],
    "color_end": THEMES["Kraken"][1],
    "color_mode": "gradient",
    "track_color": "#1C1C24",
    "brightness": 100,
    "port": None,
    "interval": 1.0,
    "liquid_sensor": "",
    "pump_sensor": "",
    "fan_sensor": "",
}


def config_path():
    base = os.environ.get("APPDATA") or os.path.expanduser("~")
    return os.path.join(base, "CoolerScreen", "config.json")


def load():
    cfg = dict(DEFAULTS)
    try:
        with open(config_path(), encoding="utf-8") as f:
            cfg.update(json.load(f))
    except (OSError, ValueError):
        pass
    if cfg["mode"] not in MODES:
        cfg["mode"] = DEFAULTS["mode"]
    if cfg["color_mode"] not in COLOR_MODES:
        cfg["color_mode"] = DEFAULTS["color_mode"]
    return cfg


def save(cfg):
    path = config_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
