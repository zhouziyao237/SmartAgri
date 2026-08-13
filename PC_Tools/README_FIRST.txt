SmartAgri Unified Upper Computer V2.4.8
========================================

Use START.bat. Web and Qt share ONE .venv.

V2.4.8 deployment:
1. MASTER STM32 USB-TTL: PA2/PA3, 115200 8N1.
2. Open Web upper computer.
3. New V2.4.8 SLAVE/UNCONFIGURED ESP can discover MASTER by true ESP-NOW broadcast; preconfigured AT+GATEWAY is not required.
4. Open "无线与节点部署" to read/set ESP Radio Config.
5. "下载完整配置 JSON" exports ESP radio + STM32 sensor config.
6. "导入配置 JSON" applies sensor config first, then radio config/reboot.

Compatibility:
- Route Protocol remains version 2.
- V2.4.2-V2.4.6 nodes remain basic route-compatible where their own firmware can reach the network.
- Remote Radio Config / zero-touch discovery are V2.4.8 capabilities and gracefully degrade on older nodes.

OTA quick use:
1. Confirm MASTER connected and target Node ONLINE.
2. Open Remote OTA.
3. Select Node ID and component.
4. Click Read Target Info.
5. Choose the matching application .bin.
6. Click Start OTA and wait for DONE + node online again.

STM32 OTA file: smart_agri_comm.bin only (never bootloader.bin).
ESP32 OTA after local build: SmartAgri_ESP32_V2.4.8.bin.
ESP32-C3 OTA after local build: SmartAgri_ESP32C3_V2.4.8.bin.

Identity rule:
- The attached ESP is authoritative for STM32 ROLE/DEVICE/NETWORK/GROUP.
- The same STM32 application is used for MASTER and SLAVE.
- Sensor factory reset preserves node identity.

MASTER PC link:
  MASTER STM32 PA2 TX -> USB-TTL RX
  MASTER STM32 PA3 RX <- USB-TTL TX
  GND <-> GND
  115200 8N1
Close any serial terminal using that USB-TTL COM before starting SmartAgri.
