import sys

from PIL import Image, ImageDraw


def create_icon(size=64):
    image = Image.new("RGBA", (64, 64), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    draw.ellipse([2, 2, 62, 62], fill=(10, 10, 14, 255))
    draw.arc([8, 8, 56, 56], 130, 410, fill=(40, 40, 50, 255), width=7)
    draw.arc([8, 8, 56, 56], 130, 320, fill=(45, 124, 255, 255), width=7)
    return image if size == 64 else image.resize((size, size), Image.LANCZOS)


if __name__ == "__main__":
    create_icon(256).save(sys.argv[1], sizes=[(16, 16), (32, 32), (48, 48), (64, 64), (256, 256)])
