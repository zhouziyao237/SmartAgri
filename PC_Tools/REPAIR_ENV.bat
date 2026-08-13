@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo [SmartAgri] This will rebuild the unified .venv used by BOTH Web and Qt.
echo Close SmartAgri Web/Qt windows first.
if exist ".venv" (
  rmdir /s /q ".venv"
  if exist ".venv" (
    echo [ERROR] Could not remove .venv. A Python process may still be using it.
    pause
    exit /b 1
  )
)
call "%~dp0scripts\ensure_env.bat"
if errorlevel 1 (
  echo [ERROR] Repair failed.
  pause
  exit /b 1
)
echo.
echo [OK] Unified environment repaired.
"%SMARTAGRI_PY%" -c "import serial,PySide6; from PySide6 import QtCore; print('Python/serial/PySide6 OK, Qt',QtCore.qVersion())"
pause
