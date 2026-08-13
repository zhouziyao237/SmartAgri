@echo off
setlocal EnableExtensions
cd /d "%~dp0.."
set "ROOT=%CD%"
set "VENV=%ROOT%\.venv"
set "VPY=%VENV%\Scripts\python.exe"

if exist "%VPY%" goto CHECK_ENV

echo [SmartAgri] Creating one unified Python environment: .venv
call :CREATE_VENV
if errorlevel 1 goto FAIL

:CHECK_ENV
"%VPY%" -c "import sys,struct; assert (3,9) <= sys.version_info[:2] <= (3,13); assert struct.calcsize('P')*8 == 64; import serial; import PySide6; from PySide6 import QtCore,QtGui,QtWidgets; print('ENV OK:',sys.version.split()[0],'Qt',QtCore.qVersion())" >nul 2>&1
if not errorlevel 1 goto OK

echo [SmartAgri] Installing/updating Web + Qt dependencies...
"%VPY%" -m pip install --upgrade pip setuptools wheel
if errorlevel 1 goto FAIL
"%VPY%" -m pip install --only-binary=:all: -r "%ROOT%\requirements-unified.txt"
if errorlevel 1 goto QT_FALLBACK
"%VPY%" -c "import serial; import PySide6; from PySide6 import QtCore,QtGui,QtWidgets; print('PySide6',PySide6.__version__,'Qt',QtCore.qVersion())"
if errorlevel 1 goto QT_FALLBACK
goto OK

:QT_FALLBACK
echo [SmartAgri] PySide6-Essentials failed; trying full PySide6 package...
"%VPY%" -m pip install --only-binary=:all: "pyserial>=3.5" "PySide6>=6.8,<7"
if errorlevel 1 goto FAIL
"%VPY%" -c "import serial; import PySide6; from PySide6 import QtCore,QtGui,QtWidgets; print('PySide6',PySide6.__version__,'Qt',QtCore.qVersion())"
if errorlevel 1 goto FAIL
goto OK

:CREATE_VENV
py -3.12 -c "import struct; assert struct.calcsize('P')*8==64" >nul 2>&1 && py -3.12 -m venv "%VENV%" && exit /b 0
py -3.11 -c "import struct; assert struct.calcsize('P')*8==64" >nul 2>&1 && py -3.11 -m venv "%VENV%" && exit /b 0
py -3.10 -c "import struct; assert struct.calcsize('P')*8==64" >nul 2>&1 && py -3.10 -m venv "%VENV%" && exit /b 0
py -3.13 -c "import struct; assert struct.calcsize('P')*8==64" >nul 2>&1 && py -3.13 -m venv "%VENV%" && exit /b 0
py -3 -c "import sys,struct; assert (3,9)<=sys.version_info[:2]<=(3,13); assert struct.calcsize('P')*8==64" >nul 2>&1 && py -3 -m venv "%VENV%" && exit /b 0
python -c "import sys,struct; assert (3,9)<=sys.version_info[:2]<=(3,13); assert struct.calcsize('P')*8==64" >nul 2>&1 && python -m venv "%VENV%" && exit /b 0
if defined IDF_PYTHON_ENV_PATH if exist "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" (
  "%IDF_PYTHON_ENV_PATH%\Scripts\python.exe" -m venv "%VENV%" && exit /b 0
)
if exist "D:\PY_ENV\Espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe" (
  "D:\PY_ENV\Espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe" -m venv "%VENV%" && exit /b 0
)
echo [ERROR] No usable 64-bit Python 3.9-3.13 found.
exit /b 1

:OK
endlocal & set "SMARTAGRI_PY=%VPY%" & set "SMARTAGRI_ROOT=%ROOT%"
exit /b 0

:FAIL
echo.
echo [ERROR] SmartAgri Python/Qt environment setup failed.
echo Run DIAGNOSE.bat and copy the full output if this repeats.
exit /b 1
