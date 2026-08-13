param(
    [string]$BuildDir = ".\cmake-build-debug",
    [string]$OpenOcdExe = "",
    [string]$ScriptsDir = "",
    [switch]$NoNrst
)
$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$BuildDir = (Resolve-Path $BuildDir).Path
$BootElf = Join-Path $BuildDir "smart_agri_bootloader.elf"
$AppElf  = Join-Path $BuildDir "smart_agri_comm.elf"
if (!(Test-Path $BootElf)) { throw "Bootloader ELF not found: $BootElf. Build smart_agri_ota_bundle first." }
if (!(Test-Path $AppElf))  { throw "Application ELF not found: $AppElf. Build smart_agri_ota_bundle first." }

if (-not $OpenOcdExe) {
    if ($env:OPENOCD_EXE -and (Test-Path $env:OPENOCD_EXE)) { $OpenOcdExe = $env:OPENOCD_EXE }
    else {
        $cmd = Get-Command openocd.exe -ErrorAction SilentlyContinue
        if ($cmd) { $OpenOcdExe = $cmd.Source }
        elseif (Test-Path "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe") { $OpenOcdExe = "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe" }
    }
}
if (-not $OpenOcdExe -or -not (Test-Path $OpenOcdExe)) { throw "OpenOCD not found. Pass -OpenOcdExe or set OPENOCD_EXE / PATH." }
$OpenOcdExe = (Resolve-Path $OpenOcdExe).Path

if (-not $ScriptsDir) {
    if ($env:OPENOCD_SCRIPTS -and (Test-Path $env:OPENOCD_SCRIPTS)) { $ScriptsDir = (Resolve-Path $env:OPENOCD_SCRIPTS).Path }
    else {
        $Candidate = Join-Path (Split-Path (Split-Path $OpenOcdExe -Parent) -Parent) "share\openocd\scripts"
        if (Test-Path $Candidate) { $ScriptsDir = (Resolve-Path $Candidate).Path }
    }
}
$Cfg = if ($NoNrst) { Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd_no_nrst.cfg" } else { Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd.cfg" }
$Args = @()
if ($ScriptsDir) { $Args += @("-s", $ScriptsDir) }
$Args += @("-f", $Cfg,
           "-c", "program {$BootElf} verify",
           "-c", "program {$AppElf} verify reset exit")
Write-Host "[V2.4 FIRST INSTALL] bootloader: $BootElf"
Write-Host "[V2.4 FIRST INSTALL] app:        $AppElf"
Write-Host "Config pages 0x0800F800/0x0800FC00 are not mass-erased by this script."
& $OpenOcdExe @Args
if ($LASTEXITCODE -ne 0) { throw "OpenOCD bundle flash failed with exit code $LASTEXITCODE" }
Write-Host "DONE: STM32 OTA bootloader + relocated app installed. Future app updates can use Web/Qt OTA."
