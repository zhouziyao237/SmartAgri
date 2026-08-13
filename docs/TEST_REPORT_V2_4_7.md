# SmartAgri V2.4.7 Validation Report

## Host regression

通过：

```text
AUTO DISCOVERY / REMOTE CONFIG / COMPAT REGRESSION PASS
CONFIG AUTHORITY REGRESSION PASS
OTA SESSION CODEC PASS
PC LOCAL LINK REGRESSION PASS
STM32 TX GATE REGRESSION PASS
UART/ROUTER V2.4.7 REGRESSION PASS
```

额外检查：

```text
python -m py_compile PC_Tools/smartagri_protocol.py PC_Tools/smartagri_server.py
node --check PC_Tools/static/app.js
```

## 覆盖重点

- Protocol-2 command IDs / Radio Config codec 一致性
- SLAVE gateway 不再是 config validation 必填项
- 自动发现 / channel scan 默认开启并从旧 NVS reserved bytes 迁移
- 无 gateway 时 ESP-NOW 真广播 fallback
- 双向 peer learning / legacy ACK binding
- MASTER 对无线 peer 的 PC presence synthesis
- STM32 本地 ESP radio config forwarding + recovery reply path
- PC Web + Qt Radio Config GET/SET / JSON export/import
- 既有 UART/Router、PC local、TX gate、config authority、OTA codec 回归

## Target build 状态

当前打包环境没有 Windows ESP-IDF/ARM target executables，未在此环境声称完成新的 V2.4.7 target cross-build。请在 Windows 使用包内 build scripts clean build；若编译器报告任何错误，以 Windows target build 为最终判据。
