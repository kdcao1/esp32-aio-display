import os
import subprocess
import sys
import tempfile

TASK_NAME = "CoolerScreen"

TASK_XML = """<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.2" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo><Description>Start Cooler Screen at logon</Description></RegistrationInfo>
  <Triggers><LogonTrigger><Enabled>true</Enabled><UserId>{user}</UserId><Delay>PT10S</Delay></LogonTrigger></Triggers>
  <Principals><Principal id="Author"><UserId>{user}</UserId><LogonType>InteractiveToken</LogonType><RunLevel>HighestAvailable</RunLevel></Principal></Principals>
  <Settings>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT0S</ExecutionTimeLimit>
    <Enabled>true</Enabled>
  </Settings>
  <Actions Context="Author"><Exec><Command>{command}</Command><Arguments>{arguments}</Arguments><WorkingDirectory>{workdir}</WorkingDirectory></Exec></Actions>
</Task>
"""


def _escape(text):
    return text.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;").replace('"', "&quot;")


def _launch_command():
    if getattr(sys, "frozen", False):
        return sys.executable, "--no-elevate", os.path.dirname(sys.executable)
    pythonw = os.path.join(os.path.dirname(sys.executable), "pythonw.exe")
    script = os.path.abspath(sys.argv[0])
    return (pythonw if os.path.exists(pythonw) else sys.executable), f'"{script}" --no-elevate', os.path.dirname(script)


def _schtasks(*args):
    flags = subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0
    return subprocess.run(["schtasks", *args], capture_output=True, text=True, creationflags=flags)


def is_enabled():
    return _schtasks("/Query", "/TN", TASK_NAME).returncode == 0


def enable():
    command, arguments, workdir = _launch_command()
    user = f"{os.environ.get('USERDOMAIN', '')}\\{os.environ.get('USERNAME', '')}".lstrip("\\")
    xml = TASK_XML.format(user=_escape(user), command=_escape(command), arguments=_escape(arguments),
                          workdir=_escape(workdir))
    fd, path = tempfile.mkstemp(suffix=".xml")
    try:
        with os.fdopen(fd, "w", encoding="utf-16") as f:
            f.write(xml)
        result = _schtasks("/Create", "/TN", TASK_NAME, "/XML", path, "/F")
    finally:
        os.remove(path)
    if result.returncode != 0:
        raise OSError(result.stderr.strip() or result.stdout.strip() or "schtasks failed")


def disable():
    result = _schtasks("/Delete", "/TN", TASK_NAME, "/F")
    if result.returncode != 0 and is_enabled():
        raise OSError(result.stderr.strip() or "schtasks failed")
