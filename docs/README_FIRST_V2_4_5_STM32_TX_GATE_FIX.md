> **历史文件：当前请以 `README_FIRST_V2_4_6_UART_ROUTER_AUDIT_FIX.md` 为准。**

# SmartAgri V2.4.5 — STM32 → ESP TX Gate Fix（历史说明）

## 这次定位到的系统性问题

V2.4.4 的 `CommLink_SendRouteRaw()` 在真正调用 USART1 TX 之前，强制执行：

```c
PB0(WAKE)=1;
等待 PB1(READY)==1，最长 50 ms;
如果 READY 仍为低，直接 return false;
```

而 ESP V2.4.2 的实现中，WAKE 只是配置成输入但没有任何业务处理，READY 只是初始化后固定拉高。也就是说这不是实际的流控协议，却被 STM32 当成了“必须满足才能发送”的硬条件。

结果是：只要 PB1/READY 未接、接错、接触不良或上电时序不合适，**STM32 USART1 完全不会发送一个字节**。RX DMA 不依赖 READY，所以会出现非常典型的非对称现象：

- ESP → STM32：可以收到，`esp_raw/esp_ok` 增长；
- STM32 → ESP：完全不发，ESP `RX_FRAMES=0`；
- MASTER 和 SLAVE 两块 STM32 用同一份固件，因此两块都会同时中招；
- 上位机“发现全部节点”必须经过 MASTER STM32 → MASTER ESP，同时 SLAVE STM32 也要把 NODE_INFO 回传给 C3，所以任意一侧 TX 被 READY 卡住都会导致 Node1001 不出现。

## V2.4.5 修复

1. **READY 改为 advisory diagnostic，不再阻断 TX。**
   - PB0 WAKE 仍会在发送期间拉高，兼容旧接线；
   - PB1 READY 仍会采样并统计；
   - READY=0 也照常通过 USART1 发送。

2. **USART1 TX 改为确定性的 blocking TX。**
   - RX 继续使用 DMA1 Channel5 circular DMA；
   - TX 使用 `HAL_UART_Transmit()`；
   - 921600 保持不变；
   - 去掉 TX DMA callback/state 对关键业务链路的依赖。

3. **新增 STM32 TX 诊断。**

新 `[STAT]` 增加：

```text
esp_tx_try=...
esp_tx_ok=...
esp_tx_bytes=...
esp_tx_fail=...
esp_tx_encfail=...
ready_hi=...
ready_lo=...
```

判定方式：

- `esp_tx_try=0`：业务逻辑没有要求 STM32 发包；
- `esp_tx_try>0, esp_tx_ok>0`：STM32 USART1 已实际完成发送；
- `ready_lo>0` 但 `esp_tx_ok` 仍增长：证明 V2.4.6 已不再被 READY 线卡死；
- `esp_tx_fail>0`：HAL USART1 TX 本身失败，需要继续查 PA9/UART 硬件。

## 需要刷什么

**只需要把两块 STM32 都升级到 V2.4.6。**

ESP32 和 ESP32-C3 可以继续保持现有 V2.4.2，不需要为了这个修复重刷。上位机协议也未改变。

两块 STM32 仍然刷完全同一个 Bootloader + App。MASTER/SLAVE 最终身份仍由 attached ESP 的 `NODE_SET` 决定。

## 接线

业务 UART 必需：

```text
Classic ESP32 GPIO17 TX -> STM32 PA10 RX
Classic ESP32 GPIO16 RX <- STM32 PA9 TX
GND                     <-> GND

ESP32-C3 GPIO7 TX       -> STM32 PA10 RX
ESP32-C3 GPIO6 RX       <- STM32 PA9 TX
GND                     <-> GND
```

V2.4.5 开始 PB0/PB1 不再是业务 UART 能否发送的硬前提：

```text
PB0 -> ESP WAKE   可保留
PB1 <- ESP READY  可保留
```

## 刷完后的验证

先让 ESP 完成 NODE_SET，STM32 应出现：

```text
[IDENTITY] source=ESP ...
esp_raw>0 esp_ok>0
```

随后打开上位机并执行“发现全部节点”。MASTER STM32 应出现：

```text
esp_tx_try>0
esp_tx_ok>0
```

SLAVE 收到远端 HEARTBEAT 后也应出现 `esp_tx_ok>0`，随后 Node1001 的 NODE_INFO 会沿 C3 -> ESP-NOW -> MASTER ESP -> MASTER STM32 -> PC 返回。

ESP 侧此时 `AT+BUS?` 的 `RX_FRAMES` 应开始增长。
