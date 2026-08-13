# MASTER / SLAVE Bring-up — V2.2.0

## 1. Radio layer

- Classic ESP32 = MASTER, Device 1, TESTMODE=0, UPLINK=NONE.
- ESP32-C3 = SLAVE, Device 1001/1002/..., TESTMODE=0.
- Same NETWORK/GROUP/CHANNEL.
- Verify C3 HELLO reaches MASTER ESP before leaving test bring-up.

## 2. STM32 <-> ESP business UART

MASTER STM32 <-> Classic ESP32: 921600 A5 on PA9/PA10 <-> GPIO16/17.

SLAVE STM32 <-> C3: 921600 A5 on PA9/PA10 <-> GPIO6/7.

With TESTMODE=0 each ESP sends NODE_SET to its attached STM32 so the STM32 role/device/network/group follows the ESP identity.

## 3. PC/Web link

Current V2.2.0 normal architecture:

```text
PC -> USB-TTL -> MASTER STM32 USART2 PA2/PA3 @115200
```

Run `PC_Tools/run_server.bat`. It automatically scans COM ports with the formal A5 protocol and selects only role=MASTER/device=1.

No MASTER ESP Wi-Fi or TCP setup is required.

## 4. Acceptance

Web must show Device 1 and remote nodes. Then verify CAPS_GET, configuration transaction, telemetry and GPIO control.
