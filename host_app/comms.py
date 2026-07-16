import serial
import serial.tools.list_ports
import time
import os

class ESP32Comms:
    def __init__(self, port=None, baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.connected = False

    def autodiscover(self):
        ports = serial.tools.list_ports.comports()
        for p in ports:
            if "CH340" in p.description or "CP210" in p.description or "USB Serial" in p.description:
                return p.device
        return None

    def connect(self):
        if not self.port:
            self.port = self.autodiscover()
        
        if self.port:
            try:
                self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
                self.connected = True
                print(f"Connected to ESP32 on {self.port}")
                return True
            except Exception as e:
                print(f"Failed to connect to {self.port}: {e}")
        return False

    def disconnect(self):
        if self.ser and self.ser.is_open:
            self.ser.close()
        self.connected = False

    def send_command(self, cmd):
        if self.connected and self.ser.is_open:
            try:
                self.ser.write((cmd + "\n").encode())
                return True
            except Exception as e:
                print(f"Serial write error: {e}")
                self.connected = False
        return False

    def set_temp_mode(self):
        return self.send_command("MODE:TEMP")

    def set_gif_mode(self):
        return self.send_command("MODE:GIF")

    def update_temp(self, temp_val):
        return self.send_command(f"TEMP:{temp_val:.1f}")

    def upload_gif(self, file_path):
        if not os.path.exists(file_path):
            return False
            
        file_size = os.path.getsize(file_path)
        print(f"Uploading GIF of size {file_size} bytes...")
        if self.send_command(f"UPLOAD:{file_size}"):
            # Wait for ACK from ESP32
            time.sleep(0.5)
            # Send file in chunks
            try:
                with open(file_path, "rb") as f:
                    while chunk := f.read(1024):
                        self.ser.write(chunk)
                        # Minimal delay to not overwhelm the serial buffer
                        time.sleep(0.01)
                print("Upload complete.")
                return True
            except Exception as e:
                print(f"Upload failed: {e}")
        return False
