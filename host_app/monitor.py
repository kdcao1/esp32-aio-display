import ctypes
import os
import sys
import time

import clr

CPU_TEMP_NAMES = ("Core (Tctl/Tdie)", "CPU Package", "Package", "Core Average", "Core Max")
GPU_TYPES = ("GpuNvidia", "GpuAmd", "GpuIntel")
LIQUID_HINTS = ("liquid", "coolant", "water")


def is_admin():
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except (AttributeError, OSError):
        return False


class Reading:
    __slots__ = ("hw_type", "hw_name", "type", "name", "value")

    def __init__(self, hw_type, hw_name, sensor_type, name, value):
        self.hw_type, self.hw_name, self.type, self.name, self.value = hw_type, hw_name, sensor_type, name, value

    @property
    def label(self):
        return f"{self.hw_name} / {self.name}"


class HardwareMonitor:
    def __init__(self, overrides=None):
        self.overrides = overrides or {}
        base = os.path.dirname(os.path.abspath(__file__))
        dll_path = os.path.join(base, "LibreHardwareMonitorLib.dll")
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"Could not find {dll_path}")
        hid_path = os.path.join(base, "HidSharp.dll")
        if os.path.exists(hid_path):
            clr.AddReference(hid_path)
        clr.AddReference(dll_path)

        from LibreHardwareMonitor.Hardware import Computer

        try:
            self.computer = self._open(Computer, controllers=True)
        except Exception as e:
            print(f"Controller sensors unavailable: {e}")
            self.computer = self._open(Computer, controllers=False)

    @staticmethod
    def _open(computer_class, controllers):
        computer = computer_class()
        computer.IsCpuEnabled = True
        computer.IsGpuEnabled = True
        computer.IsMotherboardEnabled = True
        computer.IsControllerEnabled = controllers
        computer.Open()
        return computer

    def readings(self):
        result = []

        def visit(hw, hw_type):
            hw.Update()
            for s in hw.Sensors:
                if s.Value is not None:
                    result.append(Reading(hw_type, str(hw.Name), str(s.SensorType), str(s.Name), float(s.Value)))
            for sub in hw.SubHardware:
                visit(sub, hw_type)

        for hw in self.computer.Hardware:
            visit(hw, str(hw.HardwareType))
        return result

    @staticmethod
    def _pick(readings, names):
        for name in names:
            for r in readings:
                if name.lower() in r.name.lower():
                    return r.value
        return readings[0].value if readings else None

    def _override(self, readings, key, sensor_type):
        pattern = (self.overrides.get(key) or "").lower()
        if not pattern:
            return False, None
        for r in readings:
            if r.type == sensor_type and pattern in r.label.lower():
                return True, r.value
        return True, None

    def read(self):
        all_readings = self.readings()

        def of(hw_types, sensor_type):
            return [r for r in all_readings if r.hw_type in hw_types and r.type == sensor_type]

        stats = {
            "cpu_temp": self._pick(of(("Cpu",), "Temperature"), CPU_TEMP_NAMES),
            "cpu_load": next((r.value for r in of(("Cpu",), "Load") if r.name == "CPU Total"), None),
            "gpu_temp": None,
            "gpu_load": None,
        }

        for gpu_type in GPU_TYPES:
            temps = of((gpu_type,), "Temperature")
            if temps:
                stats["gpu_temp"] = self._pick(temps, ("GPU Core", "GPU Hot Spot"))
                stats["gpu_load"] = next((r.value for r in of((gpu_type,), "Load") if r.name == "GPU Core"), None)
                break

        found, value = self._override(all_readings, "liquid_sensor", "Temperature")
        if not found:
            value = next((r.value for r in all_readings if r.type == "Temperature"
                          and any(h in r.label.lower() for h in LIQUID_HINTS)), None)
        stats["liquid_temp"] = value

        fans = [r for r in all_readings if r.type == "Fan" and r.value > 0]
        found, value = self._override(all_readings, "pump_sensor", "Fan")
        if not found:
            value = next((r.value for r in fans if "pump" in r.label.lower()), None)
        stats["pump_rpm"] = value

        found, value = self._override(all_readings, "fan_sensor", "Fan")
        if not found:
            value = next((r.value for r in fans if "pump" not in r.label.lower()), None)
        stats["fan_rpm"] = value
        return stats

    def close(self):
        self.computer.Close()


if __name__ == "__main__":
    if not is_admin():
        print("WARNING: Not running as administrator. Some sensors might not be available.")
    monitor = HardwareMonitor()
    if "--list" in sys.argv:
        for r in monitor.readings():
            print(f"{r.type:<12} {r.label:<60} {r.value:.1f}")
        sys.exit(0)
    while True:
        print(monitor.read())
        time.sleep(1)
