import sys
import clr
import os
import time
import ctypes

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

# Initialize the hardware monitor
# Must be called as administrator for full access to CPU sensors
def init_monitor():
    if not is_admin():
        print("WARNING: Not running as administrator. CPU Temps might not be available.")
    
    dll_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "LibreHardwareMonitorLib.dll"))
    if not os.path.exists(dll_path):
        raise FileNotFoundError(f"Could not find {dll_path}")

    clr.AddReference(dll_path)
    
    from LibreHardwareMonitor import Hardware
    computer = Hardware.Computer()
    computer.IsCpuEnabled = True
    computer.IsMotherboardEnabled = False
    computer.IsGpuEnabled = False
    computer.IsMemoryEnabled = False
    computer.IsNetworkEnabled = False
    computer.IsControllerEnabled = False
    computer.IsStorageEnabled = False

    computer.Open()
    return computer

def get_cpu_temp(computer):
    # Iterate through hardware
    for hardware in computer.Hardware:
        if hardware.HardwareType == hardware.HardwareType.Cpu:
            hardware.Update()
            for sensor in hardware.Sensors:
                if sensor.SensorType == sensor.SensorType.Temperature:
                    if "Core (Tctl/Tdie)" in sensor.Name or "Package" in sensor.Name or "Core Average" in sensor.Name:
                        if sensor.Value is not None:
                            return sensor.Value
            # Fallback if package temp is not found
            for sensor in hardware.Sensors:
                if sensor.SensorType == sensor.SensorType.Temperature:
                    if sensor.Value is not None:
                        return sensor.Value
    return None

if __name__ == "__main__":
    comp = init_monitor()
    while True:
        temp = get_cpu_temp(comp)
        print(f"CPU Temp: {temp}°C")
        time.sleep(1)
