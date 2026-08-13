# V2.4.7 Auto Discovery / Remote Config / Compatibility Change Report

## 新增

- ESP-NOW 真广播发现：未绑定 SLAVE/UNCONFIGURED 使用 `FF:FF:FF:FF:FF:FF`。
- Channel Scan：未绑定节点可轮询 1..13 Channel 找 MASTER。
- Auto Binding：LINK_ACK 后自动学习 MASTER MAC 和实际 Channel，并保存 NVS。
- Legacy compatibility：保持 Route Protocol 2；接受旧 zero-payload LINK_HELLO/LINK_ACK。
- LINK capability extension：新节点 HELLO/ACK 可附带 firmware/protocol range/capabilities/target/channel。
- Remote Radio Config：新增 `0x3030/0x3031/0x3032` GET/REPLY/SET。
- Unconfigured provisional ID：新节点不需要先用 AT 设置 Device ID 才能被 MASTER 看见。
- PC/Web Radio Config 页面。
- 完整配置 JSON export/import：ESP Radio + STM32 sensor slots。
- STM32 persisted-MASTER recovery 路径允许 PC 在 `WAIT_ESP` 时配置本地 ESP radio，但不放宽普通 radio routing identity 权限。

## 保持不变

- A5 outer frame / CRC8 / CRC16。
- `SA_ROUTE_VERSION = 2`。
- 同一 STM32 App 用于 MASTER/SLAVE，运行角色仍由 attached ESP `NODE_SET` 决定。
- STM32 OTA App 仍链接在 `0x08002000`，Bootloader 布局不变。
- ESP32 GPIO17/16、ESP32-C3 GPIO7/6、STM32 PA9/PA10 business UART 定义不变。

## Legacy 限制

V2.4.7 MASTER 能与 V2.4.2–V2.4.6 Protocol-2 节点通信，但无法给“从未具备广播发现能力、且没有 gateway 的旧 SLAVE 固件”凭空增加发送能力。旧 SLAVE 至少需要已有 gateway/能把 HELLO 或业务包送到 MASTER，或先升级到 V2.4.7。
