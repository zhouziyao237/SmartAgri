param(
    [string]$OpenOcdExe = "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe",
    [string]$ScriptsDir = "D:\GCC-ENV\Arm-GCC\openocd\share\openocd\scripts",
    [switch]$NoNrst
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
if (-not (Test-Path $OpenOcdExe)) { throw "OpenOCD not found: $OpenOcdExe" }
if (-not (Test-Path $ScriptsDir)) { throw "OpenOCD scripts not found: $ScriptsDir" }

$Cfg = if ($NoNrst) {
    Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd_no_nrst.cfg"
} else {
    Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd.cfg"
}

Write-Host "Testing ST-LINK/SWD with current OpenOCD..."
Write-Host "OpenOCD: $OpenOcdExe"
Write-Host "Config : $Cfg"
& $OpenOcdExe -s $ScriptsDir -f $Cfg -c "init; targets; reset halt; shutdown"
if ($LASTEXITCODE -ne 0) { throw "ST-LINK/SWD test failed with exit code $LASTEXITCODE" }
