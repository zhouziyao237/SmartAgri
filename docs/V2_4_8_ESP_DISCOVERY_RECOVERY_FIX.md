# V2.4.8 ESP Discovery Race / Lost-Gateway Recovery Fix

## Root cause fixed

V2.4.7 deferred ESP-NOW RX processing through a delivery queue. During channel scan an ACK could be received on channel 6, queued, then the scan task advanced to channel 7 before `bind_gateway_from_ack()` ran. The old code called `esp_wifi_get_channel()` at processing time and therefore persisted channel 7 even though the ACK was physically received on channel 6.

## Fixes

1. Capture ESP-NOW RX channel in `recv_cb()` from `esp_now_recv_info_t::rx_ctrl->channel`.
2. Carry that channel through the delivery queue and peer map.
3. `bind_gateway_from_ack()` uses extended ACK advertised channel first, then captured RX channel for legacy ACK. It no longer derives binding from the later current scan channel.
4. After binding, force Wi-Fi back to the ACK channel before continuing traffic.
5. Increase scan dwell to 300 ms as latency margin; correctness no longer depends on dwell timing.
6. Track valid traffic from MASTER. If an existing gateway produces no MASTER traffic for 15 seconds, clear the stale runtime/NVS binding and automatically restart full channel discovery.
7. Prevent duplicate discovery tasks.

## Compatibility

- Route protocol remains version 2.
- Extended V2.4.8/V2.4.7 LINK_ACK is supported.
- Legacy zero-payload LINK_ACK remains supported using captured RX channel.
- STM32 firmware remains V2.4.7 unchanged for this release. Only ESP32/ESP32-C3 need V2.4.8 for the discovery/recovery fix.
