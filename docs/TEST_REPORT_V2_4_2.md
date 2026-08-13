# SmartAgri V2.4.2 Local Validation Report

## Passed in packaging environment

- STM32 and ESP protocol host tests: PASS.
- OTA session codec regression: PASS.
- ESP-authoritative configuration source regression: PASS.
- Changed STM32 `config_manager.c` and `node_service.c`: host GCC syntax check with HAL stubs + `-Wall -Wextra -Werror`: PASS.
- Changed ESP `app_config.c`, `at_shell.c`, `uart_bridge.c`: host GCC syntax check with ESP-IDF API stubs + `-Wall -Wextra -Werror`: PASS.
- Python upper-computer modules `py_compile`: PASS.
- Web JavaScript syntax check: PASS.
- Web DOM page-parent regression check for overview/sensor/control/profiles/ota/settings/events: PASS.

## Important limitation

This packaging environment does not contain the user's Windows ARM-GCC/OpenOCD toolchain or ESP-IDF 5.5.1 installation. Therefore the final V2.4.2 firmware still needs the user's normal real builds:

- STM32: CLion/CMake + `arm-none-eabi-gcc` and ST-LINK flash.
- ESP32 / ESP32-C3: ESP-IDF 5.5.1 target builds and serial flash.
- Final hardware validation: attached-ESP identity sync, ESP-NOW Node 1/1001 communication, and real OTA transfer.

## First hardware acceptance criteria

1. Same STM32 binary is flashed to both STM32 boards.
2. C3 active AT config is SLAVE / 1001; attached STM32 logs `source=ESP ... device=1001` without changing wiring.
3. Classic ESP active AT config is MASTER / 1; attached STM32 logs `source=ESP ... device=1`.
4. `AT+BUS?` TX counter grows after startup sync; STM32 `esp_rx` grows above zero.
5. Node 1 and Node 1001 both become ONLINE in upper computer.
6. Node 1001 STM32 OTA proceeds PREPARE -> BOOTLOADER -> BEGIN -> TRANSFER -> VERIFY -> REBOOT -> DONE.
