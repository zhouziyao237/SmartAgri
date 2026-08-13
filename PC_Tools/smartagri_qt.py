#!/usr/bin/env python3
import os
import sys
from pathlib import Path

try:
    import PySide6
except ModuleNotFoundError:
    print("[ERROR] PySide6 is not installed in this Python environment.")
    print("Run START_QT.bat or REPAIR_ENV.bat; do not run smartagri_qt.py manually.")
    raise SystemExit(3)

# Bind Qt to the plug-ins belonging to the exact PySide6 installation used here.
_qt_root = Path(PySide6.__file__).resolve().parent
_plugin_root = _qt_root / "plugins"
_platform_root = _plugin_root / "platforms"
if _plugin_root.exists():
    os.environ["QT_PLUGIN_PATH"] = str(_plugin_root)
if _platform_root.exists():
    os.environ["QT_QPA_PLATFORM_PLUGIN_PATH"] = str(_platform_root)
if hasattr(os, "add_dll_directory"):
    try:
        os.add_dll_directory(str(_qt_root))
    except OSError:
        pass

from PySide6.QtWidgets import QApplication, QMessageBox
from qt_app.api import ApiClient
from qt_app.main_window import MainWindow

def main():
    api = ApiClient()
    try:
        backend = api.ensure_backend()
    except Exception as e:
        app = QApplication(sys.argv)
        QMessageBox.critical(None, "SmartAgri", str(e))
        return 2
    app = QApplication(sys.argv)
    win = MainWindow(api, backend)
    win.show()
    return app.exec()

if __name__ == "__main__":
    raise SystemExit(main())
