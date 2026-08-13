# SmartAgri V2.4.0 Validation Report

This report describes the checks performed in the packaging environment for the V2.4.0 OTA/extensible upper-computer package.

## Passed in this environment

- Python syntax compilation for `PC_Tools` passed.
- Web JavaScript `node --check` passed.
- Web local-object-template layout was rendered in a browser-sized preview; the old narrow right-card layout has been replaced by full-width rows with independent Node binding/actions.
- STM32 A5/ROUTE host protocol test passed with `-Wall -Wextra -Werror`.
- ESP A5/ROUTE host protocol test passed with `-Wall -Wextra -Werror`.
- OTA packed C structures verified:
  - route header = 20 bytes
  - OTA info get = 4 bytes
  - OTA info reply = 16 bytes
  - OTA prepare = 8 bytes
  - OTA begin = 16 bytes
  - OTA data head = 12 bytes
  - OTA end = 12 bytes
  - OTA simple/status = 8 bytes
  - OTA result = 16 bytes
- Python OTA packet packing and STM32/ESP image pre-validation smoke tests passed.
- STM32 bare OTA bootloader cross-compiled and linked with Clang/LLD for Cortex-M3 using freestanding/no-builtin settings. No unresolved symbols remained; image is well below the reserved 8 KB bootloader region.
- Qt source files pass Python syntax compilation. The Qt application is split into auto-discovered `qt_app/pages/*.py` page objects.

## Windows/target checks that still must be done on the user's machine

This container does **not** contain the user's Windows Arm GNU toolchain, ESP-IDF 5.5.1 toolchain, ST-LINK hardware, ESP boards, or PySide6 runtime. Therefore this report does not claim:

- a full `arm-none-eabi-gcc` build of the STM32 HAL application,
- a real ST-LINK flash/boot test,
- a real ESP-IDF build/flash for ESP32 and ESP32-C3,
- a real PySide6 Windows `qwindows.dll` launch,
- an end-to-end OTA transfer over the user's physical ESP-NOW link.

Use the included Windows scripts for those checks. If Qt fails, run `PC_Tools\run_qt_repair.bat`, then `PC_Tools\qt_diagnose.bat` and preserve the complete output. If firmware build fails, preserve the complete build log.

## OTA safety boundary

V2.4.0 provides engineering OTA transport, target probing, CRC32 integrity checks, retry/offset recovery, a fixed STM32 recovery bootloader, and ESP dual-app OTA partitions. CRC32 is not firmware authentication. Production deployment should add signed firmware/authentication and the platform security features appropriate to the product.
