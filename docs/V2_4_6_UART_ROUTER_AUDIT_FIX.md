# SmartAgri V2.4.6 UART / Router Code Audit

## 审计范围

- STM32 USART1 RX-DMA / TX、USART2 PC link、NodeService、identity/config authority、OTA handoff
- ESP32 / ESP32-C3 business UART、AT、ESP-NOW router、OTA、TCP uplink
- STM32 / ESP / PC protocol constants and host protocol codec
- Windows ESP build/flash artifact naming

## 已确认并修复

### Critical / High

1. **STM32 NODE_SET 无本地应答**：V2.4.5 收到 ESP 身份后不会调用 USART1 TX。V2.4.6 每个合法 NODE_SET 都向 `SA_DEVICE_LOCAL_ESP` 回 NODE_INFO。
2. **SLAVE 无主动存在性**：V2.4.6 已同步 SLAVE 每 5 s 主动向 PC 发 HEARTBEAT。
3. **ESP UART 假成功**：旧 `TX_FRAMES` 在没有检查 driver 返回值时也增长。V2.4.6 检查 exact byte count + TX done。
4. **ESP-NOW callback 阻塞 UART**：改成 RX callback -> queue -> worker router。
5. **stale identity**：STM32 7 s 未收到新 NODE_SET 就关闭 radio routing；persisted MASTER 只保留本地 PC 管理。
6. **LOCAL_ESP 泄漏无线**：Device 0 在 UART/router/ESP-NOW 边界被显式消费/拒绝。
7. **跨 Group 本地执行**：SLAVE radio ingress 严格 group filter；MASTER 只在 group match 时执行本地 broadcast，但仍可转发其他 group。
8. **ESP OTA session 松散**：PREPARE/BEGIN 必须使用非零且相同 transfer_id；CRC mismatch 会 abort active OTA handle 并进入 ERROR。
9. **TCP partial send / stale parser**：循环发送直至完整；reconnect 重置 parser；socket access 加 mutex。
10. **构建产物旧版本名**：ESP CMake project/bin/dist 名全部改为 V2.4.6，防止“源码新、刷进去旧名字”的误刷。

### Medium

11. STM32/ESP protocol enum 同步 `SA_CMD_DIAG_TELEMETRY=0x31F0`。
12. MASTER 本地产生的 PC-bound route 在 identity ready 时额外镜像给 attached ESP，使 `UPLINK=TCP` 能收到 MASTER 本地数据，同时 USART2 本地 PC 仍保留。
13. MASTER ESP 收到 remote PC-bound route 时，若 UPLINK=TCP，会 tee 到 TCP，同时继续送 MASTER STM32/USART2。
14. UART 诊断扩展到底层 raw/CRC/hardware event，便于区分线路、波特率和协议问题。
15. ESP UART 多任务 TX 的 sequence/attempt 统计改为 mutex 内串行，避免诊断计数竞争。
16. ESP OTA `esp_ota_write()` 失败立即 abort handle，避免错误会话继续接受 DATA。
17. STM32 Bootloader 的 OTA BEGIN 同样拒绝 `transfer_id=0`。
18. STM32 DebugLog 修复 `vsnprintf` 截断时多发 NUL 的边界问题。

## 仍需关注但本版不做大规模重构

1. STM32 SensorManager 仍有 DS18B20/BH1750/DHT 等 blocking delay。USART1 RX 是 256-byte circular DMA + main-loop drain；高业务流量和长传感器阻塞同时发生时存在 DMA 环绕丢字节风险。后续建议把慢传感器改成状态机，或增加 DMA HT/TC ISR drain。
2. Pulse counter 当前是 main-loop polling，不适合高速脉冲；后续应切 EXTI/TIM input capture。
3. ESP-NOW 当前“send API 返回成功”代表已进入 Wi-Fi TX 队列，不等于对端 MAC 已确认收到；如需链路质量统计，应增加 send callback ACK counters。
4. 本环境没有用户 Windows 上的 ARM GCC / ESP-IDF cross toolchain，因此这里能做 host codec/regression/static checks，但最终 embedded cross-build 仍必须在用户工具链执行。

## 实机判据

STM32：

```text
esp_raw>0 esp_ok>0
esp_tx_try>0 esp_tx_ok>0 esp_tx_fail=0
```

ESP：

```text
AT+BUS?
TX_TRY>0 TX_FRAMES>0 TX_WRITE_FAIL=0 TX_WAIT_FAIL=0
RX_RAW>0 RX_FRAMES>0 CRC8=0 CRC16=0 FRAME_ERR=0
```

如果 GPIO17 有实际波形但 PA10 没有，则仍是物理路径，不是 Router 逻辑。
