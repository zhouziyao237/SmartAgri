@echo off
setlocal EnableExtensions
cd /d "%~dp0"
call "%~dp0scripts\ensure_env.bat"
if errorlevel 1 goto FAIL
set "PYTHONHOME="
set "PYTHONPATH="
set "PYTHONNOUSERSITE=1"
echo [SmartAgri] Starting Qt desktop upper computer...
"%SMARTAGRI_PY%" "%~dp0smartagri_qt.py"
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" goto FAIL_RC
exit /b 0
:FAIL_RC
echo.
echo [ERROR] Qt exited with code %RC%.
echo Run DIAGNOSE.bat and copy the complete output.
pause
exit /b %RC%
:FAIL
pause
exit /b 1
