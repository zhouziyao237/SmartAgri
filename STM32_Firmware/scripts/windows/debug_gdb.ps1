param(
    [Parameter(Mandatory=$true)][string]$Elf,
    [string]$GdbExe = "",
    [switch]$ViaBootloader
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Elf = (Resolve-Path $Elf).Path

if (-not $GdbExe) {
    if ($env:ARM_GCC_ROOT) {
        $Candidate = Join-Path $env:ARM_GCC_ROOT "bin\arm-none-eabi-gdb.exe"
        if (Test-Path $Candidate) { $GdbExe = $Candidate }
    }
}
if (-not $GdbExe) {
    $cmd = Get-Command arm-none-eabi-gdb.exe -ErrorAction SilentlyContinue
    if ($cmd) { $GdbExe = $cmd.Source }
}
if (-not $GdbExe -or -not (Test-Path $GdbExe)) {
    throw "arm-none-eabi-gdb.exe not found. Pass -GdbExe or set ARM_GCC_ROOT / PATH."
}

$GdbExe = (Resolve-Path $GdbExe).Path
$InitName = if ($ViaBootloader) { "stm32f103_stlink.gdb" } else { "stm32f103_app_direct.gdb" }
$Init = Join-Path $ProjectRoot "gdb\$InitName"

Write-Host "GDB : $GdbExe"
Write-Host "ELF : $Elf"
Write-Host "Init: $Init"
& $GdbExe $Elf -x $Init
exit $LASTEXITCODE
