@echo off
setlocal EnableExtensions
cd /d "%~dp0"
:MENU
cls
echo ============================================================
echo        SmartAgri V2.4.5 Unified Upper Computer
echo ============================================================
echo.
echo   1. Start Web UI
echo   2. Start Qt Desktop UI
echo   3. Start Web + Qt together
echo   4. Repair Python/Qt environment
echo   5. Diagnostics
echo   0. Exit
echo.
set /p CHOICE=Select [0-5]: 
if "%CHOICE%"=="1" (
  call "%~dp0START_WEB.bat"
  goto MENU
)
if "%CHOICE%"=="2" (
  call "%~dp0START_QT.bat"
  goto MENU
)
if "%CHOICE%"=="3" (
  call "%~dp0START_BOTH.bat"
  goto MENU
)
if "%CHOICE%"=="4" (
  call "%~dp0REPAIR_ENV.bat"
  goto MENU
)
if "%CHOICE%"=="5" (
  call "%~dp0DIAGNOSE.bat"
  goto MENU
)
if "%CHOICE%"=="0" exit /b 0
goto MENU
