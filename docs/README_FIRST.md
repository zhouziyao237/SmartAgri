# SmartAgri V2.4.8 — ESP Discovery Race / Lost-Gateway Recovery Fix

这是当前主入口文档。V2.4.8 基于 V2.4.7，修复 Channel Scan ACK 延迟处理导致“在 CH6 收到 ACK 却保存 CH7”的竞态，并加入已绑定 Gateway 失联后的自动重扫。STM32 源码保持 V2.4.7，不需要为了本次 ESP 组网修复重新刷 STM32；协议继续为 Protocol-2。

## 1. 目标行为

正常部署不再要求每个 SLAVE 预先执行 `AT+GATEWAY=...`：

```text
SLAVE / UNCONFIGURED 上电
  -> 若未绑定 MASTER，扫描 Wi-Fi Channel 1..13
  -> 各 Channel 向 FF:FF:FF:FF:FF:FF 发送 LINK_HELLO
  -> MASTER 从 ESP-NOW recv_info 学习 SLAVE MAC / Device ID
  -> MASTER 回 LINK_ACK
  -> SLAVE 自动学习 MASTER MAC + 当前 Channel
  -> 保存 NVS
  -> 后续业务优先单播
```

Discovery 使用真实 ESP-NOW 广播地址 `FF:FF:FF:FF:FF:FF`。正常业务在 peer 已学习后走单播；广播只用于发现或目标 MAC 未知时的兼容回退。

## 2. 零配置新节点

V2.4.8 的 `UNCONFIGURED` ESP 也会启动 ESP-NOW。它会根据 STA MAC 生成临时 Device ID：

```text
0x70xxxxxx
```

MASTER 收到 HELLO 后会把这个临时节点呈现给 PC/Web。可在“无线与节点部署”页面直接下发：

- Role
- Device ID
- Network ID
- Group ID
- Channel
- Auto Discovery
- Channel Scan
- Power Mode
- Uplink
- Gateway MAC（SLAVE 可留空）

保存成功后目标 ESP 会返回 CONTROL_RESULT，然后延时重启，以新配置重新入网。

## 3. 配置下载 / 导入

Web / Qt 上位机支持：

- 读取目标 ESP Radio Config
- 下载完整 Node JSON（ESP Radio + STM32 Sensor Config）
- 导入完整 Node JSON
- 导入时先配置 STM32 Sensor Slots，最后配置 ESP Radio，避免 Device ID / Channel 提前变化导致事务中断

AT 仍保留，但定位为维护、救援和工厂调试接口，不再是普通 SLAVE 部署的必要步骤。

## 4. 兼容策略

固件版本与协议版本分离：

```text
Firmware: 2.4.2 ... 2.4.8
Route Protocol: 2
```

V2.4.8 仍使用 `SA_ROUTE_VERSION = 2`。

| 组合 | 基础 Protocol-2 路由 | 自动发现 | 远程 Radio Config | 说明 |
|---|---|---|---|---|
| 2.4.8 MASTER + 2.4.8 SLAVE | 是 | 是 | 是 | 全功能 |
| 2.4.8 MASTER + 2.4.2–2.4.7 SLAVE | 是 | 取决于旧 SLAVE 是否已有 gateway/会发 HELLO | 旧节点通常否 | MASTER 接受 legacy zero-payload HELLO |
| 2.4.8 SLAVE + 2.4.2–2.4.7 MASTER | 是 | 是，V2.4.8 SLAVE 真广播；旧 MASTER/STM32 可回 legacy ACK | MASTER 旧版本通常否 | V2.4.8 SLAVE 可从 legacy ACK 学习 MASTER |

注意：旧 V2.4.2–V2.4.6 SLAVE 本身若强制要求 gateway 且从未配置过，它不会因为 MASTER 升级而自动获得“新固件里的广播扫描能力”。这是旧端能力限制，不是 Protocol-2 不兼容。

## 5. 网络隔离

自动发现仍受以下逻辑域限制：

- `network_id` 必须一致（radio header 过滤）
- `group_id` 必须一致（route 过滤）
- 新出厂 / factory-default 节点默认 Network=1、Group=1

它不是跨 Network 的无条件广播，以避免不同 SmartAgri 网络互相绑定。

## 6. UART / Wiring 不变

### Classic ESP32 MASTER ↔ STM32

```text
ESP GPIO17 UART2_TX  -> STM32 PA10 USART1_RX
ESP GPIO16 UART2_RX  <- STM32 PA9  USART1_TX
ESP GPIO25           <- STM32 PB0 WAKE
ESP GPIO26           -> STM32 PB1 READY
GND                   <-> GND
Business UART: 921600 8N1, A5 FRAME-only
```

### ESP32-C3 SLAVE ↔ STM32

```text
C3 GPIO7 UART1_TX  -> STM32 PA10 USART1_RX
C3 GPIO6 UART1_RX  <- STM32 PA9  USART1_TX
C3 GPIO3           <- STM32 PB0 WAKE
C3 GPIO10          -> STM32 PB1 READY
GND                 <-> GND
Business UART: 921600 8N1, A5 FRAME-only
```

ESP 板载 USB-UART / UART0 只负责 ESP_LOG + AT @115200。
MASTER STM32 USART2 PA2/PA3 @115200 连接 PC/Web。

## 7. Windows Build / Flash

ESP32：

```powershell
cd .\ESP_Firmware
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
Get-ChildItem .\scripts\windows\*.ps1 | Unblock-File
.\scripts\windows\preflight.ps1
.\scripts\windows\build_flash_esp32.ps1 -Port COM12
```

ESP32-C3：

```powershell
cd .\ESP_Firmware
.\scripts\windows\build_flash_c3.ps1 -Port COM4
```

脚本会从源码重新 clean build。不要使用旧 V2.4.6 build/dist 文件。

STM32：

```powershell
cd .\STM32_Firmware
cmake --build .\cmake-build-debug --target smart_agri_ota_bundle -j
.\scripts\windows\flash_ota_bundle.ps1 -BuildDir .\cmake-build-debug
```

实际 CMake build 目录可按你的 CLion Profile 调整。

## 8. 上板验证顺序

1. MASTER ESP：`AT+GMR` 确认 `FW=2.4.8`。
2. MASTER STM32：确认 `esp_raw/esp_ok/esp_tx_ok` 持续增长，CRC8/CRC16=0。
3. 新 SLAVE 清除 gateway 或 factory reset；`AUTO=1,SCAN=1`。
4. SLAVE 启动后应打印 `[DISCOVERY] bound MASTER=... channel=...`。
5. MASTER Web / Qt 节点列表应出现 SLAVE / 临时 provisional node。
6. Web / Qt“无线与节点部署”读取 Radio Config。
7. 下载 JSON，再测试导入 JSON。
8. 最后测试 Sensor Config、业务 Telemetry、OTA。

## 9. 本包验证边界

本包在当前生成环境完成了 Python/JS/源码回归测试；ESP-IDF 和 ARM Windows 目标工具链不在当前 Linux 运行环境，因此最终 ESP/STM32 target build 需在你的 Windows 工具链执行。构建脚本会强制 clean build，避免误用历史产物。
