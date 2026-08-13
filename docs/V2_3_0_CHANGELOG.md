# V2.3.0 Changelog

## PC Upper Computer
- Reworked local backend into a single serial owner shared by Web and Qt clients.
- Added online/offline/last_seen handling and offline cache cleanup.
- Added `CONFIG_REPLY` decoding and current node configuration viewer.
- Added synchronous configuration transaction result checking.
- Added complete graphical driver/interface/pin/measurement selections including GPIO_INPUT.
- Added local JSON settings and reusable sensor profiles.
- Added GPIO output control with result feedback.
- Added Qt/PySide6 desktop application.
- Added improved responsive Web UI.

## STM32
- Firmware version 2.3.0 / capability FW 0x0203.
- Added inner command `0x31F0 SA_CMD_DIAG_TELEMETRY`.
- Diagnostic command causes the target node to generate one real `0x3100 TELEMETRY` packet and return a control result, allowing full route testing without physical sensors.

## ESP
- No required protocol change; retain the verified V2.1.8b ESP32 / ESP32-C3 firmware and production configuration.
