$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
function Need([string]$Rel,[string]$Label) {
    $P = Join-Path $Root $Rel
    if (!(Test-Path $P)) { throw "[FAIL] $Label missing: $Rel" }
    Write-Host "[OK] $Label`: $Rel"
}
Need "components\at_shell\at_shell.c" "AT shell source"
Need "components\cluster_common\include\smartagri_protocol.h" "protocol header"
Need "components\espnow_link\espnow_link.c" "ESP-NOW source"
Need "main\app_main.c" "app main"
Need "components\ota_manager\ota_manager.c" "OTA manager source"
Need "components\uart_bridge\uart_bridge.c" "UART bridge source"
Need "components\debug_console\debug_console.c" "debug console source"
Need "components\cluster_common\smartagri_protocol.c" "cluster_common CMake source"

$ClusterCmake = Get-Content (Join-Path $Root "components\cluster_common\CMakeLists.txt") -Raw
if ($ClusterCmake -notmatch 'smartagri_protocol\.c') { throw "[FAIL] cluster_common CMake source mapping" }
Write-Host "[OK] cluster_common CMakeLists source mapping"

$Dbg = Get-Content (Join-Path $Root "components\debug_console\debug_console.c") -Raw
if ($Dbg -notmatch 'CONFIG_IDF_TARGET_ESP32C3[\s\S]*DEBUG_TRANSPORT "UART0"[\s\S]*DEBUG_TX_PIN\s+21[\s\S]*DEBUG_RX_PIN\s+20') {
    throw "[FAIL] C3 debug/config UART0 GPIO21/20 mapping"
}
if ($Dbg -match 'usb_serial_jtag') { throw "[FAIL] stale C3 USB Serial/JTAG debug dependency" }
Write-Host "[OK] C3 debug/config UART0 GPIO21/20 @115200"

$Bus = Get-Content (Join-Path $Root "components\uart_bridge\uart_bridge.c") -Raw
if ($Bus -notmatch 'CONFIG_IDF_TARGET_ESP32C3[\s\S]*BUS_UART\s+UART_NUM_1[\s\S]*TX_PIN\s+7[\s\S]*RX_PIN\s+6') {
    throw "[FAIL] C3 business UART1 GPIO7/6 mapping"
}
if ($Bus -notmatch '#define BUS_BAUD 921600') { throw "[FAIL] business baud is not 921600" }
Write-Host "[OK] C3 business UART1 GPIO7/6 @921600 FRAME-only"

$C3Cfg = Get-Content (Join-Path $Root "sdkconfig.defaults.esp32c3") -Raw
if ($C3Cfg -notmatch 'CONFIG_ESP_CONSOLE_UART_DEFAULT=y') { throw "[FAIL] C3 primary console not UART0 default" }
if ($C3Cfg -match 'USB_SERIAL_JTAG') { throw "[FAIL] stale USB Serial/JTAG console config" }
Write-Host "[OK] C3 ESP-IDF primary console = UART0"

$At = Get-Content (Join-Path $Root "components\at_shell\at_shell.c") -Raw
if ($At -notmatch 'FW=2\.4\.8') { throw "[FAIL] firmware version string is not 2.4.8" }
Write-Host "[OK] firmware version = 2.4.8"

$Ota = Get-Content (Join-Path $Root "components\ota_manager\ota_manager.c") -Raw
if ($Ota -notmatch 'esp_ota_begin' -or $Ota -notmatch 'esp_ota_write' -or $Ota -notmatch 'esp_ota_set_boot_partition') { throw "[FAIL] ESP-IDF OTA API integration" }
Write-Host "[OK] ESP OTA manager uses ESP-IDF OTA APIs"

Write-Host "PREFLIGHT PASS - V2.4.8 auto-discovery/config compatibility + OTA + dual UART"
