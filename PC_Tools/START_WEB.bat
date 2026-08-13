@echo off
setlocal EnableExtensions
cd /d "%~dp0"
call "%~dp0scripts\ensure_env.bat"
if errorlevel 1 goto FAIL
set "PYTHONHOME="
set "PYTHONPATH="
set "PYTHONNOUSERSITE=1"
echo [SmartAgri] Starting Web backend at http://127.0.0.1:8080 ...
"%SMARTAGRI_PY%" "%~dp0smartagri_server.py"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto FAIL_RC
exit /b 0
:FAIL_RC
echo.
echo [ERROR] Web backend exited with code %RC%.
pause
exit /b %RC%
:FAIL
pause
exit /b 1
