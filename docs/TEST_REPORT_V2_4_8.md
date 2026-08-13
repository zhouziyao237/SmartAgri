# SmartAgri V2.4.8 Validation Report

## Scope

ESP-only discovery reliability release on top of V2.4.7. STM32 V2.4.7 source is intentionally unchanged.

## Fixed defect reproduced from hardware

Observed hardware sequence:

- MASTER ESP32: Channel 6
- SLAVE ESP32-C3 scan received MASTER ACK, but V2.4.7 printed `bound ... channel=7`

Root cause: ESP-NOW receive callback queued the ACK while the scan task advanced from CH6 to CH7. Deferred `bind_gateway_from_ack()` then read the later current Wi-Fi channel instead of the channel on which ACK was received.

## V2.4.8 changes validated statically

- ESP-NOW RX channel is captured from `esp_now_recv_info_t::rx_ctrl->channel` inside receive callback.
- Captured channel is carried in the delivery queue and peer map.
- Binding prefers extended ACK advertised channel, otherwise captured RX channel for legacy ACK.
- Binding forces Wi-Fi back to ACK channel and persists that channel.
- Existing bound SLAVE tracks valid MASTER traffic.
- If MASTER is absent for 15 seconds, stale Gateway binding is cleared and full auto-discovery restarts.
- Duplicate discovery tasks are prevented.
- Protocol remains Route Version 2.
- STM32 remains V2.4.7 unchanged.

## Regression suite

```text
AUTO DISCOVERY / REMOTE CONFIG / COMPAT REGRESSION PASS
CONFIG AUTHORITY REGRESSION PASS
OTA SESSION CODEC PASS
PC LOCAL LINK REGRESSION PASS
STM32 TX GATE REGRESSION PASS
UART/ROUTER V2.4.8 REGRESSION PASS
```

C brace-balance structural checks also passed for modified `app_main.c` and `espnow_link.c`.

## Target-build caveat

This packaging environment does not contain the user's Windows ESP-IDF 5.5.1 target toolchain, therefore it does not claim a fresh ESP32/ESP32-C3 target cross-build. Use the included Windows scripts for the final compiler/linker check and flashing. STM32 does not need rebuilding/reflashing for this release.
