# SmartAgri V2.4.6 — 先看这个

当前推荐基线：**V2.4.6 UART / Router Audit Fix**。

本版同时更新 STM32 和 ESP，不再沿用“ESP 保持 V2.4.2”的 V2.4.5 临时方案。

```text
2 × STM32      -> V2.4.6，同一份固件
Classic ESP32  -> V2.4.6
ESP32-C3       -> V2.4.6
PC/Web         -> 本包版本
```

主从身份仍由 attached ESP `NODE_SET` 决定；STM32 不编译固定主/从角色。

先阅读：`README_FIRST_V2_4_6_UART_ROUTER_AUDIT_FIX.md`
审计详情：`V2_4_6_UART_ROUTER_AUDIT_FIX.md`
