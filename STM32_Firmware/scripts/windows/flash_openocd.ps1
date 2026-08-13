param(
    [Parameter(Mandatory=$true)][string]$Elf,
    [string]$OpenOcdExe = "",
    [string]$ScriptsDir = "",
    [switch]$NoNrst
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Elf = (Resolve-Path $Elf).Path

if (-not $OpenOcdExe) {
    if ($env:OPENOCD_EXE -and (Test-Path $env:OPENOCD_EXE)) { $OpenOcdExe = $env:OPENOCD_EXE }
    else {
        $cmd = Get-Command openocd.exe -ErrorAction SilentlyContinue
        if ($cmd) { $OpenOcdExe = $cmd.Source }
        elseif (Test-Path "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe") {
            $OpenOcdExe = (Resolve-Path "D:\GCC-ENV\Arm-GCC\openocd\bin\openocd.exe").Path
        }
    }
}
if (-not $OpenOcdExe -or -not (Test-Path $OpenOcdExe)) {
    throw "OpenOCD not found. Pass -OpenOcdExe or set OPENOCD_EXE / PATH."
}
$OpenOcdExe = (Resolve-Path $OpenOcdExe).Path

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
$Args += @("-f", $Cfg, "-c", "program {$Elf} verify reset exit")

Write-Host "Flashing: $Elf"
& $OpenOcdExe @Args
if ($LASTEXITCODE -ne 0) { throw "OpenOCD flash failed with exit code $LASTEXITCODE" }
