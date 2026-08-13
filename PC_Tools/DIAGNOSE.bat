@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo ============================================================
echo SmartAgri V2.4.5 Upper Computer Diagnostics
echo ============================================================
call "%~dp0scripts\ensure_env.bat"
if errorlevel 1 goto END
set "PYTHONHOME="
set "PYTHONPATH="
set "PYTHONNOUSERSITE=1"
echo.
echo [Python]
"%SMARTAGRI_PY%" -c "import sys,platform,struct; print(sys.executable); print(sys.version); print(platform.platform()); print('bits=',struct.calcsize('P')*8)"
echo.
echo [Packages]
"%SMARTAGRI_PY%" -c "import serial,PySide6; from PySide6 import QtCore; print('pyserial=',serial.__version__); print('PySide6=',PySide6.__version__); print('Qt=',QtCore.qVersion()); print('PySide6 path=',PySide6.__file__)"
echo.
echo [Serial ports]
"%SMARTAGRI_PY%" -c "from serial.tools import list_ports; [print(p.device,'|',p.description,'|',p.hwid) for p in list_ports.comports()]"
echo.
echo [Qt quick diagnostic]
"%SMARTAGRI_PY%" "%~dp0qt_diagnose.py" --quick
echo.
echo [Backend]
"%SMARTAGRI_PY%" -c "import urllib.request,json; print(json.loads(urllib.request.urlopen('http://127.0.0.1:8080/api/status',timeout=.5).read()))" 2>nul
if errorlevel 1 echo backend not running on http://127.0.0.1:8080
:END
echo.
echo ============================================================
echo Copy ALL text above if support is needed.
echo ============================================================
pause
