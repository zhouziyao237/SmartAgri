# SmartAgri V2.4.0a build fix

This package does not change the on-wire protocol or firmware feature version. It fixes two Windows/ESP-IDF build issues found during the first ESP32-C3 V2.4.0 build.

## Fix 1: GCC 14 -Werror=misleading-indentation

`components/ota_manager/ota_manager.c` previously contained:

```c
if(cfg)g_cfg=*cfg;abort_active();
```

It is now written explicitly as:

```c
if (cfg) {
    g_cfg = *cfg;
}
abort_active();
```

This preserves the intended behavior and removes the GCC 14 `-Werror=misleading-indentation` failure.

## Fix 2: inherited Python/Conda environment

The Windows ESP build scripts now clear inherited `PYTHONHOME` and `PYTHONPATH`, disable the user site directory, and force UTF-8 before invoking the configured ESP-IDF Python executable. This reduces interference from an active Miniconda/other Python environment.

## Recommended command

```powershell
cd D:\ESP-Projects\SmartAgri_V2_4_0a_OTA_Extensible_FIX\ESP_Firmware
powershell -NoProfile -ExecutionPolicy Bypass -File ".\scripts\windows\build_flash_c3.ps1" -Port COM3
```

The script removes stale `build`, `sdkconfig`, and `sdkconfig.old` automatically.
