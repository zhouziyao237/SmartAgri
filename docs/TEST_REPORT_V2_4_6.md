# SmartAgri V2.4.6 Code Audit / Host Validation Report

Date: 2026-08-12

## Scope

Reviewed the current STM32 + ESP32/ESP32-C3 + router/OTA/TCP + PC protocol source carried forward from V2.4.5. The purpose was to distinguish physical UART faults from code-path faults and remove logic that could make a healthy UART appear silent.

## Confirmed defects fixed in V2.4.6

| Severity | Area | Defect | V2.4.6 correction |
|---|---|---|---|
| Critical | STM32 NodeService | A valid ESP `NODE_SET` updated identity but generated no STM32->ESP frame, so `esp_tx_try` could remain 0 forever | Every valid local `NODE_SET` immediately returns `NODE_INFO` to `SA_DEVICE_LOCAL_ESP` |
| High | STM32 node presence | Sensorless SLAVE had no stable proactive STM32->C3 traffic | Synchronized nodes emit 5 s heartbeat; SLAVE heartbeat routes through C3/ESP-NOW to PC |
| High | STM32 identity | `identity_ready` could remain true after ESP/UART loss | 7 s NODE_SET watchdog disables radio routing until fresh local identity sync |
| High | STM32 TX flow control | Older code made PB1 READY a hard TX gate | READY is diagnostic only; USART1 TX continues even when READY is low |
| Critical | ESP UART diagnostics | `TX_FRAMES`/`AT+SYNCSTM32:SENT` could report success without checking the driver write or TX completion | Exact `uart_write_bytes()` length and `uart_wait_tx_done()` are required before success counters/AT OK |
| Medium | ESP UART concurrency | Multiple producer tasks could race sequence/attempt accounting | TX mutex now covers attempt counter, sequence encoding, write and TX-done wait |
| High | ESP UART RX observability | No raw/hardware error counters | Added raw bytes, parser/CRC/length, FIFO/full, frame and parity counters |
| High | ESP-NOW callback | Wi-Fi receive callback could route directly into blocking UART work | Callback only reassembles/enqueues; worker task performs routing |
| High | Router | `SA_DEVICE_LOCAL_ESP=0` was not consistently isolated from radio | Device 0 is consumed at local UART and rejected at ESP-NOW boundaries |
| High | Group routing | Cross-group broadcast could be locally executed on SLAVE | SLAVE ingress group filter; MASTER executes local broadcast only on group match while retaining gateway relay |
| High | ESP OTA | PREPARE/BEGIN transfer identity was not fully enforced; CRC/write failure could leave OTA handle active | Nonzero matching transfer id required; CRC and write failure abort the active handle |
| High | STM32 bootloader OTA | BEGIN could accept transfer_id 0 in recovery path | BEGIN rejects transfer_id 0 |
| Medium | TCP uplink | One `send()` was assumed to transmit a whole frame; parser state could survive reconnect | Full send loop; parser reset on reconnect; socket mutex |
| Medium | Uplink routing | MASTER local PC-bound routes did not reach attached ESP/TCP; remote PC-bound routes did not tee cleanly | Local MASTER route mirrors to attached ESP when authoritative; MASTER ESP tees remote PC traffic to TCP and USART |
| Medium | Protocol drift | ESP header was missing diagnostic command definition | STM32 and ESP protocol files are synchronized byte-for-byte |
| Medium | Build artifact | ESP internal output retained old V2.4.1/V2.4.2 names | CMake, Windows scripts and dist names use V2.4.6 |
| Low | STM32 log | `vsnprintf` truncation could transmit the terminating NUL byte | Clamp to `sizeof(buf)-1` |

## Validation completed in this environment

- Python regression scripts:
  - `config_authority_regression_test.py`
  - `ota_session_codec_test.py`
  - `pc_local_link_regression_test.py`
  - `stm32_tx_gate_regression_test.py`
  - `uart_router_v246_regression_test.py`
- Python PC tools syntax: `py_compile`.
- Web JavaScript syntax: `node --check PC_Tools/static/app.js`.
- STM32 and ESP host protocol codec builds with host GCC using `-Wall -Wextra -Werror`.
- STM32 and ESP protocol header/source equivalence checked.
- Core ESP C files syntax-checked with host GCC and lightweight ESP-IDF stubs: UART bridge, ESP-NOW router, TCP uplink, OTA manager, app main, AT shell.
- STM32 `node_service.c` syntax-checked with lightweight HAL/board stubs.

## Important limitation

This Linux validation environment does **not** contain the user's Windows `arm-none-eabi-gcc` / ESP-IDF Xtensa/RISC-V cross toolchains. Therefore this report does not claim an embedded target build, flash, or real-hardware OTA success. Run the included Windows build scripts on the user's toolchain after extraction.

## Remaining architectural risks (not hidden)

1. `SensorManager` still contains blocking sensor transactions (notably DS18B20 ~750 ms, BH1750 ~180 ms, Modbus timeout paths). USART1 RX is circular DMA drained by the main loop, so sustained high UART traffic during long sensor reads can eventually wrap the DMA buffer. This is not the cause of the current sensorless identity issue, but should be refactored to nonblocking sensor state machines / DMA HT-TC draining before high-load deployment.
2. Pulse counting is main-loop polling, not EXTI/TIM input capture, so high-rate pulse sensors can undercount.
3. ESP-NOW `esp_now_send()` acceptance means the packet entered the Wi-Fi transmit path; delivery-quality counters still need the ESP-NOW send callback if per-peer MAC ACK statistics are required.
4. If a logic analyzer sees data on ESP TX but not at STM32 RX on the same physical connection, that remains a wiring/connector/board-level fault outside firmware.

## Expected real-hardware acceptance criteria

After both ESPs and both STM32s run V2.4.6, no PC request or sensor is needed for the local UART handshake.

STM32 should show:

```text
[IDENTITY] source=ESP ...
esp_raw > 0
esp_ok > 0
esp_tx_try > 0
esp_tx_ok > 0
esp_tx_fail = 0
```

ESP `AT+BUS?` should show:

```text
TX_TRY > 0
TX_FRAMES > 0
TX_WRITE_FAIL = 0
TX_WAIT_FAIL = 0
RX_RAW > 0
RX_FRAMES > 0
CRC8 = 0
CRC16 = 0
FRAME_ERR = 0
```

Then Node1001 should have an autonomous heartbeat path:

```text
SLAVE STM32 -> C3 -> ESP-NOW -> MASTER ESP -> MASTER STM32 -> PC/Web
```
