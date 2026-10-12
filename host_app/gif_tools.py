import io

from PIL import Image, ImageOps, ImageSequence

SIZE = 480
MAX_FRAMES = 300


def prepare_gif(path):
    with Image.open(path) as src:
        frames, durations = [], []
        for i, frame in enumerate(ImageSequence.Iterator(src)):
            if i >= MAX_FRAMES:
                break
            rgba = frame.convert("RGBA")
            background = Image.new("RGBA", rgba.size, (0, 0, 0, 255))
            rgb = Image.alpha_composite(background, rgba).convert("RGB")
            frames.append(ImageOps.fit(rgb, (SIZE, SIZE), Image.LANCZOS))
            durations.append(max(int(frame.info.get("duration", 100) or 100), 20))

    out = io.BytesIO()
    frames[0].save(out, format="GIF", save_all=True, append_images=frames[1:],
                   duration=durations, loop=0, disposal=1, optimize=True)
    return out.getvalue()
