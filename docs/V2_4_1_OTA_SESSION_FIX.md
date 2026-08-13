# SmartAgri V2.4.1 OTA Session / Identity Fix

## 修复目标

V2.4.0b 在 STM32 App 收到 `OTA_PREPARE` 后会软件复位进入 Bootloader，但 Bootloader 只依赖普通配置 Flash 推断 Device ID。若节点仍在 RAM 默认配置（典型：SLAVE / Device 1001 / Group 1，generation=0），Bootloader 可能退回 Device 1，导致上位机继续发送给 Device 1001 的 `OTA_BEGIN` 无人应答。

V2.4.1 改为明确的 OTA Session：

1. App 收到 `OTA_PREPARE`。
2. 把 `device_id / group_id / transfer_id / checksum` 写入 STM32F1 BKP DR1~DR8。
3. 软件复位。
4. Bootloader 优先恢复 OTA Session 身份，再监听 `OTA_STATUS/BEGIN/DATA/END`。
5. OTA 成功或 ABORT 后清除 Session。
6. 如果掉电导致 BKP Session 丢失，Bootloader 回退读取 Config A/B；ConfigManager 现在保证首次 ESP 身份同步即使等于 RAM 默认值也会写入 Flash（generation 从 0 变为 >0）。

## 额外可靠性修复

- Bootloader 对 Config A/B 增加 schema / size / valid / CRC32 校验，不再只看 magic。
- Bootloader 支持收到新的 `OTA_PREPARE` 后更新 recovery session，便于中断后的重新任务。
- 上位机在 `OTA_PREPARE` 后不再固定 sleep 0.9s，而是轮询 `OTA_STATUS`，确认 Bootloader 已恢复同一个 transfer/session 后再发 `OTA_BEGIN`。
- 上位机自动使用节点最近广播的 Group ID，不再把 OTA 固定写死为 Group 1。
- OTA_END 成功后，上位机会继续等待组件重新响应并显示重连状态。
- ESP OTA 的 PREPARE / ABORT / STATUS 状态机做了幂等与 transfer-id 修正。

## 版本

- STM32 App / Bootloader OTA info: `0x00020401`
- ESP32 / ESP32-C3 OTA info: `0x00020401`
- PC Web / Qt: V2.4.1

## 兼容性

A5 Route 与 `0x3400~0x3408` OTA 命令号没有变化。V2.4.1 上位机可以继续发现旧 V2.4 节点，但当前 STM32 OTA_BEGIN 问题必须通过更新 STM32 Bootloader + App 才能根治。
