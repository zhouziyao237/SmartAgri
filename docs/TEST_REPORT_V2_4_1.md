# SmartAgri V2.4.1 Test Report

## 已完成静态/Host 验证

- STM32 A5/Route host protocol test: PASS
- ESP A5/Route host protocol test: PASS
- STM32 OTA Bootloader Cortex-M3 clang syntax: PASS
- STM32 OTA Bootloader freestanding Cortex-M3 link into 8 KB region: PASS
- `sa_node_config_t` layout: size=384, device_id offset=4, group_id offset=10: PASS
- OTA BKP session codec Device 1 / 1001 / 1002 + multiple Group/Transfer IDs: PASS
- PC Python `compileall`: PASS
- Web `app.js` `node --check`: PASS
- PC OTA image validation / Node group selection: PASS
- PC `PREPARE -> bootloader OTA_STATUS` polling logic mock test: PASS

## 仍需真实硬件验证

本环境没有用户 Windows ARM-GCC/ESP-IDF/ST-LINK/真实两节点硬件，因此以下必须在实际设备上跑：

1. ARM-GCC 完整 STM32 V2.4.1 build。
2. 两块 STM32 ST-LINK 首次安装 Bootloader + App。
3. Node 1001 `generation > 0`。
4. Node 1001 STM32 OTA：`PREPARE -> BOOTLOADER -> BEGIN -> TRANSFER -> VERIFY -> REBOOT -> DONE`。
5. Node 1 STM32 OTA。
6. ESP32-C3 / Classic ESP32 V2.4.1 IDF build + local flash。
7. ESP32-C3 / Classic ESP32 remote OTA。
