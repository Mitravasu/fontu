"""Fail if the deployed GUI exits during startup with Qt paths removed."""

import os
from pathlib import Path
import subprocess
import sys


executable = Path(sys.argv[1]).resolve()
environment = os.environ.copy()
for key in list(environment):
    if key.startswith(("QT_", "QML", "DYLD_")) or key in ("LD_LIBRARY_PATH", "QTDIR"):
        del environment[key]
if sys.platform == "win32":
    system = Path(environment["SystemRoot"])
    environment["PATH"] = os.pathsep.join([str(system / "System32"), str(system)])
else:
    environment["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
environment["APPIMAGE_EXTRACT_AND_RUN"] = "1"
with subprocess.Popen([str(executable)], cwd=executable.parent, env=environment) as process:
    try:
        result = process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        print("Packaged application survived startup without Qt environment paths")
    else:
        raise RuntimeError(f"Packaged application exited during startup: {result}")
