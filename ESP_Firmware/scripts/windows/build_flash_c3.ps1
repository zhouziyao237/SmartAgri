param(
  [string]$IdfPath = "D:\esp-idf",
  [string]$Python = "D:\PY_ENV\Espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe",
  [string]$IdfToolsPath = "D:\PY_ENV\Espressif",
  [string]$RomElfDir = "D:\PY_ENV\Espressif\tools\esp-rom-elfs\20241011",
  [string]$Port = "COM3",
  [int]$Baud = 460800
)
$ErrorActionPreference = "Stop"

# Keep the ESP-IDF Python environment isolated from an active Conda/user Python environment.
Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
$env:PYTHONNOUSERSITE = "1"
$env:PYTHONUTF8 = "1"
$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Assert-LastExit([string]$Step) {
  if ($LASTEXITCODE -ne 0) {
    throw "$Step failed with exit code $LASTEXITCODE"
  }
}

function Assert-ProjectFiles {
  $required = @(
    "components\cluster_common\CMakeLists.txt",
    "components\cluster_common\smartagri_protocol.c",
    "components\cluster_common\include\smartagri_protocol.h",
    "components\app_config\app_config.c",
    "components\at_shell\at_shell.c",
    "components\debug_console\debug_console.c",
    "components\uart_bridge\uart_bridge.c",
    "components\espnow_link\espnow_link.c",
    "components\tcp_uplink\tcp_uplink.c",
    "main\app_main.c",
    "components\ota_manager\ota_manager.c"
  )
  foreach ($rel in $required) {
    if (-not (Test-Path (Join-Path $root $rel))) {
      throw "Missing required source file: $rel"
    }
  }
}

Push-Location $root
try {
  Assert-ProjectFiles

  if (-not (Test-Path $Python)) { throw "Python not found: $Python" }
  if (-not (Test-Path (Join-Path $IdfPath "tools\idf.py"))) { throw "idf.py not found under: $IdfPath" }

  $env:IDF_PATH = $IdfPath
  $env:IDF_TOOLS_PATH = $IdfToolsPath
  $pythonScripts = Split-Path $Python -Parent
  $env:IDF_PYTHON_ENV_PATH = Split-Path $pythonScripts -Parent
  if (Test-Path $RomElfDir) { $env:ESP_ROM_ELF_DIR = $RomElfDir }

  if (Test-Path ".\build") { Remove-Item ".\build" -Recurse -Force }
  if (Test-Path ".\sdkconfig") { Remove-Item ".\sdkconfig" -Force }
  if (Test-Path ".\sdkconfig.old") { Remove-Item ".\sdkconfig.old" -Force }

  Write-Host "[1/3] set-target esp32c3"
  & $Python (Join-Path $IdfPath "tools\idf.py") "set-target" "esp32c3"
  Assert-LastExit "idf.py set-target esp32c3"
  if (-not (Test-Path ".\sdkconfig")) { throw "set-target returned success but sdkconfig was not created" }
  $sdk = Get-Content ".\sdkconfig" -Raw
  if ($sdk -notmatch 'CONFIG_IDF_TARGET="esp32c3"') {
    throw "sdkconfig target verification failed: expected esp32c3"
  }

  Write-Host "[2/3] build"
  & $Python (Join-Path $IdfPath "tools\idf.py") "build"
  Assert-LastExit "idf.py build"
  if (-not (Test-Path ".\build\project_description.json")) {
    throw "build verification failed: build\project_description.json missing"
  }
  $AppBin = ".\build\smartagri_esp_dualtarget_cluster_v2_4_8.bin"
  if (-not (Test-Path $AppBin)) { throw "OTA application bin missing: $AppBin" }
  New-Item -ItemType Directory -Force -Path ".\dist" | Out-Null
  Copy-Item -Force $AppBin ".\dist\SmartAgri_ESP32C3_V2.4.8.bin"
  Write-Host "OTA app bin: dist\SmartAgri_ESP32C3_V2.4.8.bin"

  Write-Host "[3/3] flash $Port"
  & $Python (Join-Path $IdfPath "tools\idf.py") "-p" $Port "-b" "$Baud" "flash"
  Assert-LastExit "idf.py flash"

  Write-Host "DONE: esp32c3 really built and flashed on $Port"
} finally {
  Pop-Location
}
