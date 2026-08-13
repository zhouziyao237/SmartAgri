# SmartAgri V2.4.6 — UART / Router 全链路审计修复

这版不是只改 STM32。它基于 V2.4.5 对 **STM32 + Classic ESP32 + ESP32-C3 + ESP-NOW Router + OTA/TCP 边界** 做了一轮完整代码审计，重点修复“ESP 在发、STM32 不主动回；TX_FRAMES 看起来在涨但不能证明真的发；Node1001 不主动出现”的逻辑链。

## 这版必须升级什么

```text
MASTER STM32      -> V2.4.6
SLAVE STM32       -> V2.4.6
Classic ESP32     -> V2.4.6
ESP32-C3          -> V2.4.6
PC/Web            -> 本包版本（协议 v2，兼容原界面）
```

两块 STM32 仍刷**完全相同**的 Bootloader + App。MASTER/SLAVE 身份仍只由 attached ESP 的 `NODE_SET` 决定。

## V2.4.5 确认存在的关键逻辑 Bug

1. STM32 收到合法 `NODE_SET` 后只更新身份，**不会给本地 ESP 回 ACK/NODE_INFO**。所以即使 RX 正常，`esp_tx_try` 也可能永久为 0。
2. 无传感器 SLAVE 没有稳定主动上报路径；不收到 PC/无线命令时可能完全没有 STM32->C3 业务帧，Node1001 的发现依赖外部触发。
3. STM32 `identity_ready` 一旦置位，ESP 后续掉线不会失效，存在 stale identity/radio routing 风险。
4. ESP V2.4.2 `uart_bridge_send_route()` **不检查** `uart_write_bytes()` 实际写入长度，也不检查 `uart_wait_tx_done()`；`AT+SYNCSTM32:SENT OK` 和 `TX_FRAMES++` 可能是假成功。
5. ESP UART 缺少 `RX_RAW/CRC/FRAME_ERR/FIFO_OVF` 等底层诊断，无法区分“没到引脚 / UART错误 / A5错误”。
6. ESP-NOW RX callback 直接进入 router，再可能阻塞在 UART TX；这是 Wi-Fi task context 中不应出现的阻塞路径。
7. `SA_DEVICE_LOCAL_ESP=0` 没有在所有无线边界强制隔离，理论上可能泄漏到 ESP-NOW。
8. STM32 / ESP 协议头曾对 `SA_CMD_DIAG_TELEMETRY` 不一致。
9. 逻辑 Group 原先没有在 SLAVE 无线入口严格过滤，跨组 broadcast 可能进入本地节点。
10. TCP uplink `send()` 假定一次写完，并且断线重连后 parser 可能携带旧半帧。
11. ESP OTA PREPARE/BEGIN session 约束不完整；BEGIN 会丢掉 PREPARE transfer_id 约束，CRC 失败也没有可靠 abort 当前 OTA handle。
12. ESP build/flash 脚本仍把新源码产物命名为旧 `v2_4_1 / V2.4.2`，很容易刷错文件。

## V2.4.6 关键行为

### 1. 本地 UART 变成真正双向握手

ESP 每 2 秒：

```text
ESP -> STM32 : NODE_SET
```

STM32 每次收到合法 NODE_SET 后立即：

```text
STM32 -> LOCAL_ESP : NODE_INFO
```

所以不需要传感器、不需要上位机，也能验证双向 UART。

正确状态应当是：

```text
STM32: esp_raw > 0, esp_ok > 0
STM32: esp_tx_try > 0, esp_tx_ok > 0
ESP:   RX_RAW > 0, RX_FRAMES > 0
```

### 2. SLAVE 主动存在性上报

已同步身份的 SLAVE 每 5 秒向 PC 发送 HEARTBEAT：

```text
SLAVE STM32 -> C3 -> ESP-NOW -> MASTER ESP -> MASTER STM32 -> PC
```

因此 Node1001 不再完全依赖刚好收到一条发现广播才出现。

### 3. ESP 的 `SENT OK` 现在才是真正可用的判断

`AT+BUS?` 新增：

```text
TX_TRY
TX_FRAMES
TX_BYTES
TX_WRITE_FAIL
TX_WAIT_FAIL
TX_ENC_FAIL
RX_RAW
RX_FRAMES
RX_DROPS
CRC8
CRC16
LENFAIL
FIFO_OVF
BUF_FULL
FRAME_ERR
PARITY_ERR
```

只有 `uart_write_bytes()` 写满整帧且 `uart_wait_tx_done()` 成功，才增加 `TX_FRAMES` 并返回 `AT+SYNCSTM32:SENT OK`。

### 4. READY 不再锁死 TX

PB1 READY 只保留诊断意义。READY=0 不会禁止 PA9 USART1 TX。

### 5. ESP-NOW Router

- Wi-Fi RX callback 只重组并入队；真正 routing 在 worker task 执行。
- `Device 0 / LOCAL_ESP` 永不进入 ESP-NOW。
- SLAVE 严格过滤自身 `group_id`。

## 正确业务接线

```text
Classic ESP32 MASTER:
GPIO17 TX -> STM32 PA10 RX
GPIO16 RX <- STM32 PA9  TX
GND       <-> GND

ESP32-C3 SLAVE:
GPIO7 TX  -> STM32 PA10 RX
GPIO6 RX  <- STM32 PA9  TX
GND       <-> GND
```

USART1 business UART = `921600 8N1`。

PC/Web：

```text
STM32 PA2 TX -> USB-TTL RX
STM32 PA3 RX <- USB-TTL TX
GND          <-> GND
115200 8N1
```

## 第一次实机验证顺序

1. 先刷两块 STM32 V2.4.6。
2. 再刷 Classic ESP32 V2.4.6、C3 V2.4.6。
3. 两个 ESP 的 AT 口分别确认 `AT+STATUS?`。
4. 两个 ESP 都执行 `AT+BUS?`。
5. STM32 必须看到 `[IDENTITY] source=ESP`。
6. 等 2~5 秒，不操作上位机也应该出现 `esp_tx_try/esp_tx_ok` 增长。
7. ESP `AT+BUS?` 应该同时看到 `RX_RAW/RX_FRAMES` 增长。
8. 最后打开 PC/Web，连接 MASTER STM32 USART2，再“发现全部节点”。

## 硬件边界

固件能修逻辑，但不能让不存在的物理波形出现。如果：

```text
ESP GPIO17 用逻辑分析仪/TTL 能看到数据
STM32 PA10 同一时刻完全看不到数据
```

那仍然是 GPIO17 -> PA10 的线、排针、共地或板级路径问题，不属于固件修复范围。
