import threading
import time
import zlib

import serial
import serial.tools.list_ports

ESPRESSIF_VID = 0x303A
BRIDGE_HINTS = ("CH340", "CH343", "CP210", "USB Serial", "USB-SERIAL", "USB JTAG")
CHUNK = 4096


class UploadError(Exception):
    pass


class ESP32Comms:
    def __init__(self, port=None, baudrate=921600):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.lock = threading.RLock()
        self._last_attempt = 0.0
        self._pending = b""

    @property
    def connected(self):
        return self.ser is not None and self.ser.is_open

    def _candidates(self):
        ports = list(serial.tools.list_ports.comports())
        if self.port:
            return [self.port]
        preferred = [p.device for p in ports if p.vid == ESPRESSIF_VID]
        others = [p.device for p in ports if p.device not in preferred
                  and any(h.lower() in (p.description or "").lower() for h in BRIDGE_HINTS)]
        return preferred + others

    def _open(self, device):
        ser = serial.Serial()
        ser.port = device
        ser.baudrate = self.baudrate
        ser.timeout = 0.2
        ser.write_timeout = 2
        ser.dtr = False
        ser.rts = False
        ser.open()
        return ser

    def _readline(self, ser, deadline):
        while b"\n" not in self._pending:
            if time.time() > deadline:
                return None
            self._pending += ser.read(max(1, ser.in_waiting))
        line, self._pending = self._pending.split(b"\n", 1)
        return line.decode(errors="ignore").strip()

    def _handshake(self, ser):
        ser.reset_input_buffer()
        self._pending = b""
        ser.write(b"PING\n")
        deadline = time.time() + 1.5
        while (line := self._readline(ser, deadline)) is not None:
            if line.startswith("PONG COOLERSCREEN"):
                return True
        return False

    def connect(self):
        with self.lock:
            if self.connected:
                return True
            self._last_attempt = time.time()
            for device in self._candidates():
                try:
                    ser = self._open(device)
                except (serial.SerialException, OSError):
                    continue
                try:
                    if self._handshake(ser):
                        self.ser = ser
                        print(f"Connected to Cooler Screen on {device}")
                        return True
                except (serial.SerialException, OSError):
                    pass
                ser.close()
            return False

    def ensure_connected(self, retry_interval=3.0):
        if self.connected:
            return True
        if time.time() - self._last_attempt < retry_interval:
            return False
        return self.connect()

    def disconnect(self):
        with self.lock:
            if self.ser:
                try:
                    self.ser.close()
                except (serial.SerialException, OSError):
                    pass
            self.ser = None

    @property
    def device(self):
        return self.ser.port if self.connected else None

    def send_command(self, cmd):
        with self.lock:
            if not self.connected:
                return False
            try:
                self.ser.write((cmd + "\n").encode())
                return True
            except (serial.SerialException, OSError) as e:
                print(f"Serial write error: {e}")
                self.disconnect()
                return False

    def set_mode(self, mode):
        return self.send_command(f"MODE:{mode}")

    def set_theme(self, c0, c1):
        return self.send_command(f"THEME:{c0.lstrip('#')},{c1.lstrip('#')}")

    def set_brightness(self, percent):
        return self.send_command(f"BRIGHT:{int(percent)}")

    def set_color_mode(self, value):
        return self.send_command(f"COLORMODE:{value}")

    def set_track(self, color):
        return self.send_command(f"TRACK:{color.lstrip('#')}")

    def send_stats(self, stats):
        keys = ("cpu_temp", "cpu_load", "gpu_temp", "gpu_load", "liquid_temp", "pump_rpm", "fan_rpm")
        values = [stats.get(k) for k in keys]
        return self.send_command("STAT:" + ",".join("" if v is None else f"{v:.1f}" for v in values))

    def _expect(self, prefix, timeout):
        deadline = time.time() + timeout
        while (line := self._readline(self.ser, deadline)) is not None:
            if line.startswith(prefix):
                return line
            if line.startswith("ERR"):
                raise UploadError(line)
        raise UploadError(f"Timed out waiting for {prefix}")

    def upload_gif(self, data, progress=None):
        with self.lock:
            if not self.connected:
                raise UploadError("Device not connected")
            try:
                self.ser.reset_input_buffer()
                self._pending = b""
                self.ser.write(f"UPLOAD:{len(data)},{zlib.crc32(data):08X}\n".encode())
                self._expect("READY", 5)
                sent = 0
                while sent < len(data):
                    chunk = data[sent:sent + CHUNK]
                    self.ser.write(chunk)
                    sent += len(chunk)
                    self._expect("ACK", 10)
                    if progress:
                        progress(sent / len(data))
                self._expect("OK", 15)
            except (serial.SerialException, OSError) as e:
                self.disconnect()
                raise UploadError(str(e))
