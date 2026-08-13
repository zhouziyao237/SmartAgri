param(
    [string]$Repo = "D:\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.6"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Board = Join-Path $ProjectRoot "boards\base\SmartAgri_F103C8"
$Dst = Join-Path $Board "Drivers"

$Need = Join-Path $Repo "Drivers\STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_adc.c"
if (!(Test-Path $Need)) {
    throw "STM32CubeF1 repository not found or incomplete: $Repo"
}

New-Item -ItemType Directory -Force -Path $Dst | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $Dst "CMSIS\Device\ST") | Out-Null

Write-Host "Copying STM32F1 HAL into board-local Drivers..."
Copy-Item -Recurse -Force (Join-Path $Repo "Drivers\STM32F1xx_HAL_Driver") $Dst
Copy-Item -Recurse -Force (Join-Path $Repo "Drivers\CMSIS\Include") (Join-Path $Dst "CMSIS")
Copy-Item -Recurse -Force (Join-Path $Repo "Drivers\CMSIS\Device\ST\STM32F1xx") (Join-Path $Dst "CMSIS\Device\ST")

$Checks = @(
  (Join-Path $Dst "STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_adc.c"),
  (Join-Path $Dst "STM32F1xx_HAL_Driver\Src\stm32f1xx_hal_adc_ex.c"),
  (Join-Path $Dst "CMSIS\Include\core_cm3.h"),
  (Join-Path $Dst "CMSIS\Device\ST\STM32F1xx\Include\stm32f103xb.h")
)
foreach ($f in $Checks) { if (!(Test-Path $f)) { throw "Vendor copy incomplete: $f" } }

@"
Source repository: $Repo
Copied by scripts/windows/prepare_board_local_drivers.ps1
Build CMake uses board-local files only.
"@ | Set-Content -Encoding UTF8 (Join-Path $Dst ".smartagri_vendor_info.txt")

Write-Host "OK: board-local STM32CubeF1 Drivers prepared at:"
Write-Host $Dst
Write-Host "Delete cmake-build-debug and Reload CMake Project."
