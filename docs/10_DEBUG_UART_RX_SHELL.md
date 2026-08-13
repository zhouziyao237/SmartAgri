# 10 — USART2 双向 Debug Console

V2.1.7 把 USART2 从“只输出 Log”升级为真正的双向调试串口。

## 接线

```text
STM32F103C8T6              USB-TTL (3.3 V logic)
--------------------------------------------------
PA2 / USART2_TX  --------> RX
PA3 / USART2_RX  <-------- TX
GND              --------- GND
```

串口参数：`115200 8N1`。

USB-TTL 的 TX 必须接 PA3，RX 必须接 PA2，TX/RX 交叉。

## 实现方式

- PA2：Log / CLI reply TX。
- PA3：Debug command RX。
- USART2 RX 使用 `HAL_UART_Receive_IT()` 单字节中断接收。
- IRQ 只把字符放入 128-byte ring buffer；命令解析在主循环 `DebugShell_Process()` 中完成。
- 不在 IRQ 中执行 printf、Flash、Sensor 或复杂协议逻辑。

## 命令

```text
help
status
clock
config
stats
sensors
reboot
```

示例：

```text
> status
config.role=SLAVE(2) device=1001 network=1 group=1 gen=0 sensors=0
comm.rx=0 comm.drop=0 dbg.rx_bytes=7 dbg.overflow=0
clock.source=HSI64
clock.sys=64000000 hclk=64000000 pclk1=32000000 pclk2=64000000
clock.hse_on=0 hse_ready=0 pll_on=1 pll_ready=1 attempts=3
clock.warn=0x1001 hse_status=3
```

如果你的 8 MHz HSE 已恢复正常，则 `clock.source=HSE72`，`clock.sys=72000000`。

## 注意

USART2 Debug Console 与以下链路可以同时工作：

```text
SWD PA13/PA14/NRST     -> ST-LINK/OpenOCD/GDB
USART2 PA2/PA3         -> USB-TTL Debug Console
USART1 PA9/PA10        -> ESP32-C3 business link
```

三条链路互不复用引脚。
