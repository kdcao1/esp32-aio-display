import ctypes
import os
import time

import clr

CPU_TEMP_NAMES = ("Core (Tctl/Tdie)", "CPU Package", "Package", "Core Average", "Core Max")


def is_admin():
    try:
        return bool(ctypes.windll.shell32.IsUserAnAdmin())
    except (AttributeError, OSError):
        return False


class HardwareMonitor:
    def __init__(self):
        dll_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "LibreHardwareMonitorLib.dll")
        if not os.path.exists(dll_path):
            raise FileNotFoundError(f"Could not find {dll_path}")
        clr.AddReference(dll_path)

        from LibreHardwareMonitor.Hardware import Computer, HardwareType
        self.HardwareType = HardwareType

        self.computer = Computer()
        self.computer.IsCpuEnabled = True
        self.computer.IsGpuEnabled = True
        self.computer.Open()

    def _collect(self, hardware):
        hardware.Update()
        readings = {}
        for hw in [hardware, *hardware.SubHardware]:
            if hw is not hardware:
                hw.Update()
            for s in hw.Sensors:
                if s.Value is not None:
                    readings.setdefault(str(s.SensorType), {})[str(s.Name)] = float(s.Value)
        return readings

    @staticmethod
    def _pick(values, names):
        for name in names:
            for key, value in values.items():
                if name in key:
                    return value
        return next(iter(values.values()), None)

    def read(self):
        HT = self.HardwareType
        stats = {"cpu_temp": None, "cpu_load": None, "gpu_temp": None, "gpu_load": None}
        gpus = []
        for hw in self.computer.Hardware:
            if hw.HardwareType == HT.Cpu and stats["cpu_temp"] is None:
                readings = self._collect(hw)
                stats["cpu_temp"] = self._pick(readings.get("Temperature", {}), CPU_TEMP_NAMES)
                stats["cpu_load"] = readings.get("Load", {}).get("CPU Total")
            elif hw.HardwareType in (HT.GpuNvidia, HT.GpuAmd, HT.GpuIntel):
                gpus.append(hw)

        gpus.sort(key=lambda h: h.HardwareType == HT.GpuIntel)
        for hw in gpus:
            readings = self._collect(hw)
            temps = readings.get("Temperature", {})
            if not temps:
                continue
            stats["gpu_temp"] = self._pick(temps, ("GPU Core", "GPU Hot Spot"))
            stats["gpu_load"] = readings.get("Load", {}).get("GPU Core")
            break
        return stats

    def close(self):
        self.computer.Close()


if __name__ == "__main__":
    if not is_admin():
        print("WARNING: Not running as administrator. CPU temps might not be available.")
    monitor = HardwareMonitor()
    while True:
        print(monitor.read())
        time.sleep(1)
