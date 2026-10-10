"""Check startup isolation against the Windows environment dictionary shape."""

import os
from pathlib import Path
import runpy
import subprocess
import sys
import unittest
from unittest.mock import patch


class SmokePackageTests(unittest.TestCase):
    def test_windows_startup_uses_uppercase_systemroot(self):
        executable = Path("fontu.exe").resolve()
        environment = {
            "SYSTEMROOT": "/windows",
            "PATH": "/qt/bin",
            "QT_PLUGIN_PATH": "/qt/plugins",
            "QML2_IMPORT_PATH": "/qt/qml",
            "QTDIR": "/qt",
        }
        with patch.object(sys, "platform", "win32"), \
                patch.object(sys, "argv", ["smoke_package.py", str(executable)]), \
                patch.dict(os.environ, environment, clear=True), \
                patch("subprocess.Popen") as launch:
            process = launch.return_value.__enter__.return_value
            process.wait.side_effect = [subprocess.TimeoutExpired(str(executable), 5), 0]
            runpy.run_path(str(Path(__file__).resolve().parents[1] / "scripts/smoke_package.py"))

        launch.assert_called_once_with(
            [str(executable)], cwd=executable.parent,
            env={"SYSTEMROOT": "/windows",
                 "PATH": os.pathsep.join([str(Path("/windows") / "System32"),
                                          str(Path("/windows"))]),
                 "APPIMAGE_EXTRACT_AND_RUN": "1"},
        )
        process.terminate.assert_called_once_with()
        self.assertEqual(process.wait.call_args_list[0].kwargs, {"timeout": 5})


if __name__ == "__main__":
    unittest.main()
