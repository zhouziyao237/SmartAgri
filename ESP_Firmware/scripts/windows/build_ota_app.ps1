param(
  [ValidateSet('esp32','esp32c3')][string]$Target = 'esp32c3',
  [string]$IdfPath = "D:\esp-idf",
  [string]$Python = "D:\PY_ENV\Espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe",
  [string]$IdfToolsPath = "D:\PY_ENV\Espressif",
  [string]$RomElfDir = "D:\PY_ENV\Espressif\tools\esp-rom-elfs\20241011"
)
$ErrorActionPreference = 'Stop'

# Keep the ESP-IDF Python environment isolated from an active Conda/user Python environment.
Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
$env:PYTHONNOUSERSITE = "1"
$env:PYTHONUTF8 = "1"
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
Push-Location $Root
try {
  if (!(Test-Path $Python)) { throw "Python not found: $Python" }
  if (!(Test-Path (Join-Path $IdfPath 'tools\idf.py'))) { throw "idf.py not found: $IdfPath" }
  $env:IDF_PATH=$IdfPath; $env:IDF_TOOLS_PATH=$IdfToolsPath
  $pythonScripts=Split-Path $Python -Parent; $env:IDF_PYTHON_ENV_PATH=Split-Path $pythonScripts -Parent
  if (Test-Path $RomElfDir) { $env:ESP_ROM_ELF_DIR=$RomElfDir }

  if (Test-Path '.\build') { Remove-Item '.\build' -Recurse -Force }
  if (Test-Path '.\sdkconfig') { Remove-Item '.\sdkconfig' -Force }
  if (Test-Path '.\sdkconfig.old') { Remove-Item '.\sdkconfig.old' -Force }

  & $Python (Join-Path $IdfPath 'tools\idf.py') 'set-target' $Target
  if ($LASTEXITCODE -ne 0) { throw "set-target $Target failed" }
  & $Python (Join-Path $IdfPath 'tools\idf.py') 'build'
  if ($LASTEXITCODE -ne 0) { throw "build $Target failed" }

  $Src='.\build\smartagri_esp_dualtarget_cluster_v2_4_8.bin'
  if (!(Test-Path $Src)) { throw "Application bin not found: $Src" }
  New-Item -ItemType Directory -Force -Path '.\dist' | Out-Null
  $Name = if ($Target -eq 'esp32') { 'SmartAgri_ESP32_V2.4.8.bin' } else { 'SmartAgri_ESP32C3_V2.4.8.bin' }
  $Dst=Join-Path '.\dist' $Name
  Copy-Item -Force $Src $Dst
  Write-Host "DONE: OTA application image -> $Dst" -ForegroundColor Green
  Write-Host 'Upload this file from Web/Qt Remote OTA after probing the target.'
} finally { Pop-Location }
