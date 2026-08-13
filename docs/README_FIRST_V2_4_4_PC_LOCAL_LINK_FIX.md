# SmartAgri V2.4.4 — MASTER PC Local Link Decouple Fix

## 这版解决什么

V2.4.3 中，MASTER STM32 只有收到 attached ESP 的 `NODE_SET` 后才把 `identity_ready` 置为 true。
PC/Web 的 A5 入口也被同一个 `identity_ready` 门控，因此当 MASTER 的 ESP↔STM32 UART1 临时故障时：

- USART2 物理串口仍然能输出 `[BOOT]/[STAT]`；
- 上位机也能打开 COM；
- 但 MASTER STM32 会丢弃 PC 的 HEARTBEAT 探测，导致上位机显示 `opened, but no MASTER STM32 A5 identity reply`。

V2.4.4 将 **本地 PC 管理链路** 与 **无线 ESP 身份/路由链路** 解耦。

## 新策略

若 Flash 中存在有效的 persisted MASTER identity（例如 role=MASTER, device=1）：

- USART2 PC/Web 本地管理立即可用；
- HEARTBEAT / NODE_INFO / CAPS / CONFIG / GPIO / STM32 本地 OTA 可在 `WAIT_ESP` 时工作；
- ESP/radio 相关转发仍然严格要求 attached ESP `NODE_SET` 成功；
- `WAIT_ESP` 时不会把任何 PC 广播或远程请求转发到 USART1/ESP-NOW；
- ESP OTA 在 `WAIT_ESP` 时不会被盲目转发。

因此 ESP/UART1 故障时，上位机至少仍可连接 Node 1 并进行本地诊断，而不会把无线故障误判为 PC 串口故障。

## USART2 新诊断

主循环 `[STAT]` 新增：

```text
pc_raw=... pc_ok=... pc_drop=... pc_tx=...
```

- `pc_raw`：USART2 实际收到的原始字节数；
- `pc_ok`：成功解析的 A5 帧数；
- `pc_drop`：Parser/ring 丢弃统计；
- `pc_tx`：STM32 向 PC 发送的 A5 帧数。

Debug shell `stats` 还会输出 `pc.crc8 / pc.crc16 / pc.lenfail / pc.uart_err`。

## 现在要刷什么

本版必需更新：

```text
MASTER STM32 -> V2.4.4 Bootloader + App
SLAVE  STM32 -> V2.4.4 Bootloader + App
```

两块 STM32 仍然刷 **完全相同** 的固件。

ESP 没有改协议，可继续：

```text
Classic ESP32 -> V2.4.2
ESP32-C3      -> V2.4.2
```

## 烧录

在 `STM32_Firmware`：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
Get-ChildItem .\scripts\windows\*.ps1 | Unblock-File

.\scripts\windows\flash_ota_bundle.ps1 `
  -BuildDir .\cmake-build-debug `
  -NoNrst
```

如果 ST-LINK 已连接 NRST，去掉 `-NoNrst`。

## MASTER 预期日志

即使 ESP UART1 仍未同步，只要 MASTER 有有效 persisted identity，应看到：

```text
[BOOT] SmartAgri STM32F103C8 V2.4.4 OTA App
[CFG-FALLBACK] role=1 device=1 ... persisted=1
[PC] local MASTER management enabled from persisted identity; radio remains WAIT_ESP until ESP NODE_SET
```

启动上位机并连接 MASTER STM32 USART2 后：

```text
pc_raw > 0
pc_ok  > 0
pc_tx  > 0
```

上位机应能显示：

```text
MASTER COMxx @ 115200
Node 1 ONLINE
```

若无线链路仍未恢复，Node 1001 可以保持 OFFLINE；这与 MASTER 本地 PC 链路不再互相阻塞。

## ESP 身份恢复后

收到 attached ESP `NODE_SET` 后仍会出现：

```text
[IDENTITY] source=ESP role=1->1 device=1->1 ...
```

此后才开放完整无线转发：Node 1001、远程控制、远程 OTA、Telemetry。
