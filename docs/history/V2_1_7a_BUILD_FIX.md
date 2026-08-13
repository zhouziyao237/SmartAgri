# V2.1.7a BUILD FIX

This package fixes the ESP-IDF CMake generation failure seen in V2.1.7.

## Root cause

`components/cluster_common/CMakeLists.txt` incorrectly registered `cluster_common.c`, but the actual protocol implementation file is `smartagri_protocol.c`. ESP-IDF therefore stopped at CMake generation with `Cannot find source file ... cluster_common.c`.

## Fixed

- `cluster_common/CMakeLists.txt` now compiles `smartagri_protocol.c`.
- Explicit `app_config`/ESP-IDF component dependencies were added to `espnow_link`, `tcp_uplink`, and `at_shell` to prevent follow-on include path errors.
- `build_flash_c3.ps1` and `build_flash_esp32.ps1` now stop immediately when any `idf.py` stage fails instead of continuing into build/flash after a failed CMake step.
- Build scripts set `IDF_PATH`, `IDF_TOOLS_PATH`, `IDF_PYTHON_ENV_PATH`, and `ESP_ROM_ELF_DIR` using the known local defaults (all are still parameters).
- Removed duplicate manual `SDKCONFIG_DEFAULTS` injection; ESP-IDF target-specific `sdkconfig.defaults.<target>` loading is left to the normal build flow.
- Added `scripts/windows/preflight.ps1`.

## First command

```powershell
cd ESP_Firmware
.\scripts\windows\preflight.ps1
.\scripts\windows\build_flash_c3.ps1 -Port COM3
```

For classic ESP32:

```powershell
.\scripts\windows\build_flash_esp32.ps1 -Port COM10
```

Firmware behavior/protocol remains V2.1.7; this is a build/package hotfix.
