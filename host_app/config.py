import json
import os

THEMES = {
    "Kraken": ("#2D7CFF", "#B22CFF"),
    "Heat": ("#00D0FF", "#FF3B30"),
    "Mint": ("#00FFA3", "#00B4FF"),
    "Sunset": ("#FFB000", "#FF2D6F"),
    "Mono": ("#E6E6EB", "#E6E6EB"),
}

MODES = ("CPU", "GPU", "DUAL", "GIF")
MODE_LABELS = {"CPU": "CPU", "GPU": "GPU", "DUAL": "CPU + GPU", "GIF": "GIF"}

DEFAULTS = {
    "mode": "CPU",
    "theme": "Kraken",
    "color_start": THEMES["Kraken"][0],
    "color_end": THEMES["Kraken"][1],
    "brightness": 100,
    "port": None,
    "interval": 1.0,
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
    return cfg


def save(cfg):
    path = config_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as f:
        json.dump(cfg, f, indent=2)
