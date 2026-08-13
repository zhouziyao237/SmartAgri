# V2.1.7c build fix

The real ESP-IDF 5.5.1 build reached 887/1004 and stopped because GCC 14.2.0 treated two `-Wformat-truncation` diagnostics as errors in `at_shell.c`.

Fixes:
- Replaced `snprintf(dst, size, "%s", src)` config copies with explicit length validation + `memcpy`.
- SSID max 32 bytes, password max 64 bytes, TCP host max 63 bytes; overlength input is rejected instead of silently truncated.
- Replaced Wi-Fi `wifi_config_t` SSID/password `snprintf` copies in `app_main.c` with validated byte copies to avoid the next likely truncation warning.
- Kept `-Werror`; warnings were fixed rather than suppressed.
- Added `AT+STA=` alias for `AT+WIFI=` and `AT+TCPSERVER=` alias for `AT+TCP=`.
- Tightened MAC octet and TCP port validation.
- `preflight.ps1` now checks that the unsafe copy patterns are absent.
- `idf_cli_check.ps1` now exports the same IDF Python environment variables as the build scripts.

The packaging environment does not contain the user's ESP-IDF 5.5.1 Windows toolchain, so the target build must still be run on the user's machine.
