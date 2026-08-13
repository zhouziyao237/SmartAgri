@echo off
setlocal EnableExtensions
cd /d "%~dp0"
call "%~dp0scripts\ensure_env.bat"
if errorlevel 1 goto FAIL
set "PYTHONHOME="
set "PYTHONPATH="
set "PYTHONNOUSERSITE=1"

"%SMARTAGRI_PY%" -c "import urllib.request; urllib.request.urlopen('http://127.0.0.1:8080/api/status',timeout=.35).read()" >nul 2>&1
if errorlevel 1 (
  echo [SmartAgri] Starting shared backend...
  start "SmartAgri Backend" /min "%SMARTAGRI_PY%" "%~dp0smartagri_server.py" --no-browser
  timeout /t 2 /nobreak >nul
) else (
  echo [SmartAgri] Existing backend detected; reusing it.
)
start "" "http://127.0.0.1:8080"
echo [SmartAgri] Starting Qt desktop UI...
"%SMARTAGRI_PY%" "%~dp0smartagri_qt.py"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto FAIL_RC
exit /b 0
:FAIL_RC
echo.
echo [ERROR] Qt exited with code %RC%.
pause
exit /b %RC%
:FAIL
pause
exit /b 1
