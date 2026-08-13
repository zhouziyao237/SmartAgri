# V2.4.5 STM32 TX Gate Fix

Root cause: `CommLink_SendRouteRaw()` in V2.4.4 treated ESP READY/PB1 as mandatory, even though the ESP V2.4.2 firmware does not implement a real WAKE/READY flow-control state machine. A low/missing READY prevented USART1 TX entirely while RX remained functional.

Changes:
- READY is sampled for diagnostics but never blocks TX.
- USART1 TX uses deterministic blocking HAL transmit; RX remains circular DMA.
- Added TX attempt/success/byte/failure and READY-high/low counters.
- Kept 921600 8N1 and the same STM32 image for MASTER/SLAVE.
- No protocol schema change; ESP V2.4.2 and existing upper computer remain compatible.
