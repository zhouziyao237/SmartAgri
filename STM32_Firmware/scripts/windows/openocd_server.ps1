param(
    [string]$OpenOcdExe = "",
    [string]$ScriptsDir = "",
    [switch]$NoNrst
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

function Resolve-OpenOcdExe {
    param([string]$Requested)
    if ($Requested) {
        if (-not (Test-Path $Requested)) { throw "OpenOCD executable not found: $Requested" }
        return (Resolve-Path $Requested).Path
    }
    if ($env:OPENOCD_EXE -and (Test-Path $env:OPENOCD_EXE)) { return (Resolve-Path $env:OPENOCD_EXE).Path }
    $cmd = Get-Command openocd.exe -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $known = "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe"
    if (Test-Path $known) { return (Resolve-Path $known).Path }
    throw "OpenOCD not found. Pass -OpenOcdExe or set OPENOCD_EXE / PATH."
}

$OpenOcdExe = Resolve-OpenOcdExe $OpenOcdExe

if (-not $ScriptsDir) {
    if ($env:OPENOCD_SCRIPTS -and (Test-Path $env:OPENOCD_SCRIPTS)) {
        $ScriptsDir = (Resolve-Path $env:OPENOCD_SCRIPTS).Path
    } else {
        $Candidate = Join-Path (Split-Path (Split-Path $OpenOcdExe -Parent) -Parent) "share\openocd\scripts"
        if (Test-Path $Candidate) { $ScriptsDir = (Resolve-Path $Candidate).Path }
    }
}

$Cfg = if ($NoNrst) {
    Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd_no_nrst.cfg"
} else {
    Join-Path $ProjectRoot "openocd\stm32f103_stlink_swd.cfg"
}

$Args = @()
if ($ScriptsDir) { $Args += @("-s", $ScriptsDir) }
$Args += @("-f", $Cfg)

Write-Host "OpenOCD : $OpenOcdExe"
Write-Host "Config  : $Cfg"
if ($ScriptsDir) { Write-Host "Scripts : $ScriptsDir" }
Write-Host "GDB port: 3333"
& $OpenOcdExe @Args
exit $LASTEXITCODE
