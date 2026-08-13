param(
  [string]$IdfPath = "D:\esp-idf",
  [string]$Python = "D:\PY_ENV\Espressif\python_env\idf5.5_py3.10_env\Scripts\python.exe",
  [string]$IdfToolsPath = "D:\PY_ENV\Espressif"
)
$ErrorActionPreference = "Stop"

# Keep the ESP-IDF Python environment isolated from an active Conda/user Python environment.
Remove-Item Env:PYTHONHOME -ErrorAction SilentlyContinue
Remove-Item Env:PYTHONPATH -ErrorAction SilentlyContinue
$env:PYTHONNOUSERSITE = "1"
$env:PYTHONUTF8 = "1"
$env:IDF_PATH = $IdfPath
$env:IDF_TOOLS_PATH = $IdfToolsPath
$pythonScripts = Split-Path $Python -Parent
$env:IDF_PYTHON_ENV_PATH = Split-Path $pythonScripts -Parent
Write-Host "Python: $Python"
Write-Host "IDF:    $IdfPath"
& $Python (Join-Path $IdfPath "tools\idf.py") "--version"
if ($LASTEXITCODE -ne 0) { throw "idf.py --version failed: $LASTEXITCODE" }
& $Python (Join-Path $IdfPath "tools\idf.py") "--list-targets"
if ($LASTEXITCODE -ne 0) { throw "idf.py --list-targets failed: $LASTEXITCODE" }
Write-Host "IDF CLI ARGUMENT TEST PASS"
