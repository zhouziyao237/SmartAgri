# 11 — V2.1.8b 最小联调顺序

1. 烧 ESP32-C3 与 ESP32。
2. 直接使用两块板自己的 USB Debug/Config COM，115200，先确认 `AT -> OK`。
3. ESP32 配 MASTER；`AT+MAC?` 记录 MAC。
4. C3 配 SLAVE + MASTER MAC。
5. `TESTMODE=1` 验证双向 ESP-NOW。
6. 两边 `TESTMODE=0`。
7. 再连接 STM32 921600 Business UART。
8. STM32 USART2 PA2/PA3 仍是独立 115200 DebugShell。
9. 最后再在 MASTER 开 Wi-Fi/TCP 上位机链路。

注意：V2.1.8b 不需要断开 STM32 Business UART 才能改 ESP AT，因为 AT 已经完全移到 Debug/Config Console。
