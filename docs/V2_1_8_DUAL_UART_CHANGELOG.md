# V2.1.8 Dual-UART Changelog

- Added `debug_console` component.
- ESP32-C3 primary UART0 / 板载 USB-UART now carries ESP_LOG + interactive AT input/output.
- Classic ESP32 primary UART0 carries ESP_LOG + interactive AT input/output.
- Business UART is now binary-only and fixed at 921600 8N1.
- Removed AT parser from business UART path.
- ESP-NOW test messages (`+LINKRX`, `+TESTRX`, `+ROUTERX`) are printed on Debug/Config console.
- `AT+PINS?` reports both config-console and business-UART mappings.
- `AT+STATUS?` reports `BUS=FRAME` and debug-console transport.
- `AT+SERIALMODE=AUTO/FRAME` retained for backward compatibility but always resolves to `FRAME_FIXED`.
- Config version bumped to `0x02010800`, so incompatible old NVS config falls back to V2.1.8 defaults.
- `at_send.ps1` default baud changed to 115200.
- Added `at_probe.ps1`.
- Updated wiring and ESP-NOW bring-up documentation.
