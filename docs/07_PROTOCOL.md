# SmartAgri protocol V2

All formal links use the same stream frame:

```text
SOF 0xA5
payload_length uint16 LE
seq uint8
header CRC8
cmd_id uint16 LE
payload
CRC16-CCITT
```

For cluster traffic `cmd_id = SA_CMD_ROUTE (0x00E1)` and payload is a route envelope:

```text
version / flags / priority
src_device
dst_device
group_id
inner_cmd
payload_len
payload
```

V2.2.0 uses this same formal binary framing on three transport segments:

1. STM32 <-> ESP business UART at 921600.
2. ESP32 MASTER <-> ESP32-C3 nodes over ESP-NOW after route-envelope conversion.
3. PC <-> MASTER STM32 USART2 at 115200.

The PC link is not AT. ASCII diagnostics may be present on MASTER STM32 TX, but the host parser ignores non-A5 bytes and resynchronizes on validated frames.

Routing rule:

```text
PC destination terminates at MASTER STM32.
Remote device destination goes MASTER STM32 -> MASTER ESP -> ESP-NOW.
Remote node -> PC goes C3 -> MASTER ESP -> MASTER STM32 -> PC.
```
