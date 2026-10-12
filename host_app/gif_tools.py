import io
import os
import re

from PIL import Image, ImageOps, ImageSequence

FULL_SIZE = 480
HALF_SIZE = 240
MAX_FRAMES = 300


def prepare_gif(path, size=FULL_SIZE):
    with Image.open(path) as src:
        frames, durations = [], []
        for i, frame in enumerate(ImageSequence.Iterator(src)):
            if i >= MAX_FRAMES:
                break
            rgba = frame.convert("RGBA")
            background = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
            rgb = Image.alpha_composite(background, rgba).convert("RGB")
            frames.append(ImageOps.fit(rgb, (size, size), Image.LANCZOS))
            durations.append(max(int(frame.info.get("duration", 100) or 100), 20))

    out = io.BytesIO()
    frames[0].save(out, format="GIF", save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, disposal=1, optimize=True)
    return out.getvalue()


def gif_name(path):
    name = re.sub(r"[^a-z0-9_-]+", "-", os.path.splitext(os.path.basename(path))[0].lower()).strip("-")
    return name[:40] or "gif"
