# SmartAgri V2.3.0 Test Report

Generated in the build workspace before packaging.

## Passed

- Python syntax compile:
  - `PC_Tools/smartagri_protocol.py`
  - `PC_Tools/smartagri_server.py`
  - `PC_Tools/smartagri_qt.py`
- Web JavaScript syntax: Node.js `--check` passed.
- Backend smoke test with a mock serial provider:
  - `/api/status`
  - `/api/schema`
  - `/api/settings`
  - `/api/profiles`
- Python A5/ROUTE diagnostic packet round-trip passed.
- C/Python packed structure agreement:
  - sensor slot = 46 bytes
  - node config = 384 bytes
  - diagnostic telemetry = 12 bytes
  - telemetry = 40 bytes
- STM32 A5 host protocol test passed with `-Wall -Wextra -Werror`.
- ESP A5 host protocol test passed with `-Wall -Wextra -Werror`.
- Modified `STM32_Firmware/app/node/node_service.c` host syntax check passed with `-Wall -Wextra -Werror` and a minimal board stub.

## Not claimed

- This environment does not contain the user's Windows STM32/ESP toolchains, so a real STM32 target build/flash and real ESP-IDF target build/flash are not claimed here.
- PySide6 runtime was not installed in the packaging container. `smartagri_qt.py` passed Python compilation; `run_qt.bat` installs PySide6 in the local venv on Windows.

## Hardware expectation

For the new no-sensor manual telemetry button, both MASTER and SLAVE STM32 devices must run V2.3.0. ESP32 and ESP32-C3 can retain the already verified V2.1.8b firmware/configuration.
