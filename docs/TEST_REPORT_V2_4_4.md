# SmartAgri V2.4.4 Test Report

## Automated checks executed in packaging environment

PASS:

- Python upper-computer modules: `py_compile`
- Web JavaScript: `node --check`
- OTA session codec regression
- Config authority regression
- PC local-link decouple source regression
- STM32 host A5 protocol test (`-Wall -Wextra -Werror`)
- ESP host A5 protocol test (`-Wall -Wextra -Werror`)

## Not executed in packaging environment

The packaging environment does not contain the user's Windows ARM GCC / OpenOCD installation or the physical STM32/ESP hardware, so the following require the user's hardware validation:

- Cortex-M3 cross compile/link with the user's Arm GCC 11.2 toolchain
- ST-LINK programming
- real USART2 PC probe
- ESP-NOW end-to-end routing
- real OTA transfer

## Required first hardware validation

1. Flash both STM32 boards with V2.4.4.
2. Keep ESP32/ESP32-C3 on V2.4.2.
3. On MASTER with ESP UART1 intentionally still unavailable, verify the log contains `[PC] local MASTER management enabled...`.
4. Close the serial monitor and let the upper computer own MASTER USART2.
5. Verify `MASTER COMxx @ 115200` and Node 1 ONLINE.
6. Restore/fix MASTER ESP UART1 and verify `[IDENTITY] source=ESP`, then discover Node 1001.
