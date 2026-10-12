import contextlib
import io
import os
import struct
import sys

APP_DESC_OFFSET = 0x20
APP_DESC_MAGIC = 0xABCD5432
ELF_SHA_OFFSET = 0xB0
APP_ADDRESS = 0x10000


def bundled_firmware():
    base = getattr(sys, "_MEIPASS", os.path.dirname(os.path.abspath(__file__)))
    for path in (os.path.join(base, "firmware", "firmware.bin"),
                 os.path.join(base, "..", "esp32_app", ".pio", "build", "waveshare_2_8c", "firmware.bin")):
        if os.path.exists(path):
            return os.path.abspath(path)
    return None


def firmware_id(path):
    with open(path, "rb") as f:
        data = f.read(ELF_SHA_OFFSET + 8)
    if len(data) < ELF_SHA_OFFSET + 8 or struct.unpack_from("<I", data, APP_DESC_OFFSET)[0] != APP_DESC_MAGIC:
        raise ValueError("Not a Cooler Screen firmware image")
    return data[ELF_SHA_OFFSET:ELF_SHA_OFFSET + 8].hex()


def flash(port, path):
    import esptool

    firmware_id(path)
    log = io.StringIO()
    args = ["--chip", "esp32s3", "--port", port, "--baud", "921600", "--before", "default_reset",
            "--after", "hard_reset", "write_flash", "-z", hex(APP_ADDRESS), path]
    try:
        with contextlib.redirect_stdout(log), contextlib.redirect_stderr(log):
            esptool.main(args)
    except (Exception, SystemExit) as e:
        tail = "\n".join(log.getvalue().strip().splitlines()[-4:])
        raise RuntimeError(f"{e}\n{tail}".strip())
