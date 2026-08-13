# PC Web connection — V2.2.0

V2.2.0 does not require MASTER ESP Wi-Fi/TCP for the normal local Web console.
The PC connects to the **MASTER STM32 USART2** through USB-TTL.

## Wiring

```text
PC USB -> USB-TTL RX <- MASTER STM32 PA2 TX
PC USB -> USB-TTL TX -> MASTER STM32 PA3 RX
PC GND ---------------- MASTER STM32 GND
```

115200 8N1.

## Protocol

This is not the STM32 debug-shell configuration mode and not ESP AT. The PC backend sends SmartAgri A5 binary frames with `SA_CMD_ROUTE=0x00E1`.

The UART TX stream may also contain human-readable diagnostic lines. `FrameParser` ignores text and resynchronizes on valid `0xA5` frames.

## One-click Windows start

Double-click:

```text
PC_Tools\run_server.bat
```

First launch creates `.venv` and installs `pyserial`. Later launches reuse the environment.

Backend behavior:

```text
scan COM ports
 -> send formal A5 HEARTBEAT request to Device 1
 -> validate NODE_INFO/HEARTBEAT reply role=MASTER device=1
 -> hold that COM as PC uplink
 -> receive all node telemetry
 -> Web UI localhost:8080
```

No `ipconfig`, fixed IP, router, ESP TCP configuration, or manual COM selection is needed.

Optional manual override for engineering:

```powershell
python smartagri_server.py --port COM8
```

## Node discovery

Every STM32 node emits a heartbeat every 5 s. The Web page therefore fills automatically. The `发现全部节点` button additionally sends a broadcast HEARTBEAT through MASTER STM32 -> MASTER ESP -> ESP-NOW.
