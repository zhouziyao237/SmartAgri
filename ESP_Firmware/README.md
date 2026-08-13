# SmartAgri ESP Dual-Target V2.4.8 OTA

同一份源码支持 ESP32-C3 (`idf.py set-target esp32c3`) 与 Classic ESP32 / ESP32-WROOM (`idf.py set-target esp32`)。

## 两个完全独立的串口/控制台

| Target | Debug / Config | 用途 | Business UART | TX | RX | 用途 |
|---|---|---|---|---:|---:|---|
| ESP32-C3 | UART0 @115200, GPIO21/20 via board USB-UART | ESP_LOG + AT | UART1 @ 921600 | GPIO7 | GPIO6 | STM32 A5 Frame |
| ESP32 | UART0 @ 115200, GPIO1/3 | ESP_LOG + AT | UART2 @ 921600 | GPIO17 | GPIO16 | STM32 A5 Frame |

V2.4.8 的 Business UART 固定为 FRAME-only。ASCII AT 不再经过 GPIO6/7 或 GPIO16/17。

ESP32-WROVER 某些模块 GPIO16/17 可能被 PSRAM 使用，需要修改 `components/uart_bridge/uart_bridge.c` 的 ESP32 引脚。

## Debug/AT

烧录后直接打开板载 USB COM。发送 CRLF：

```text
AT
AT+GMR
AT+PINS?
AT+STATUS?
AT+MAC?
```

PowerShell：

```powershell
.\scripts\windows\at_probe.ps1 -Port COM3
.\scripts\windows\at_send.ps1 -Port COM3 -File ..\configs\esp32c3_slave_1001_serial_test.txt
```

`at_send.ps1` 默认 115200；C3 与 ESP32 的板载 USB-UART Debug/AT 终端都使用 115200 8N1。

## Business UART

只连接 STM32：921600 8N1，A5 帧 + CRC8/CRC16。不会输出 ESP_LOG，也不会处理文本 AT。

## V2.4 Remote OTA

V2.4 新增 `components/ota_manager`。同一协议支持 Classic ESP32 和 ESP32-C3；OTA 页面会先查询目标类型。

项目 partition table 已包含：

```text
otadata
ota_0
ota_1
```

第一次必须通过 USB/串口烧 V2.4.8，之后 Web/Qt 可上传该 target 的 ESP-IDF **application .bin** 进行远程更新。

远程 OTA 不更新 ESP bootloader / partition table。

## V2.4.8 True Broadcast Auto Discovery

SLAVE / UNCONFIGURED 节点在 `AUTO=1` 且无 gateway 时，不再因 `SLAVE_GATEWAY_REQUIRED` 停止发送。它会使用 ESP-NOW `FF:FF:FF:FF:FF:FF` 广播 LINK_HELLO；`SCAN=1` 时扫描 Channel 1..13。收到 Device 1 的 LINK_ACK 后自动保存 MASTER MAC 和实际 Channel。

维护查询：

```text
AT+GMR
AT+STATUS?
AT+DISCOVERY?
AT+AUTODISCOVERY=1
AT+CHANNELSCAN=1
AT+GATEWAY=CLEAR
```

正常部署可直接通过 Web 的 Radio Config 完成，不需要逐节点输入 AT。
