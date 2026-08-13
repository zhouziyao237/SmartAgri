# V2.2.0 changelog

- Added `STM32_Firmware/app/comm/pc_link.c/.h`.
- MASTER STM32 USART2 PA2/PA3 now accepts formal SmartAgri A5/ROUTE frames from PC while retaining engineering text output.
- MASTER STM32 terminates all `dst=SA_DEVICE_PC` traffic locally instead of sending it back to MASTER ESP for TCP uplink.
- PC->remote-node routes are forwarded by MASTER STM32 to USART1 / MASTER ESP / ESP-NOW.
- PC broadcast is processed by MASTER and forwarded once to ESP-NOW nodes.
- MASTER identity change (`NODE_SET`) immediately announces NODE_INFO to PC for fast auto-discovery.
- PC backend changed from mandatory TCP to automatic serial discovery using pyserial.
- No manual COM selection is required by default; no `ipconfig`, PC IP, external Wi-Fi, or TCP address is required.
- Existing ESP V2.1.8b radio/AT/UART pin assignments are intentionally unchanged.
