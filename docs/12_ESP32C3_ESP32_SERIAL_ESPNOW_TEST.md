# 12 — ESP32-C3 + Classic ESP32 双串口 / ESP-NOW 联调

V2.1.8b 的核心原则：**Debug/Config 与 STM32 Business UART 完全分离。**

## 1. 接口分工

### ESP32-C3

- 板载 UART0 / 板载 USB-UART：ESP_LOG + AT 配置；终端按 115200 使用。
- UART1 GPIO7(TX) / GPIO6(RX)：921600 8N1；只收发 SmartAgri A5 二进制帧。

### Classic ESP32

- 板载 USB-UART / UART0 GPIO1(TX) / GPIO3(RX)：115200；ESP_LOG + AT 配置。
- UART2 GPIO17(TX) / GPIO16(RX)：921600 8N1；只收发 SmartAgri A5 二进制帧。

因此组网测试阶段无需把 USB-TTL 接到 921600 Business UART。

## 2. 推荐角色

```text
Classic ESP32 = MASTER, Device 1
ESP32-C3      = SLAVE,  Device 1001
Network       = 1
Group         = 1
Channel       = 6
```

## 3. 烧录

C3：

```powershell
.\scripts\windows\build_flash_c3.ps1 -Port COM3
```

ESP32：

```powershell
.\scripts\windows\build_flash_esp32.ps1 -Port COM10
```

COM 号以设备管理器实际显示为准。

## 4. 先验证 Debug/AT 控制台

烧录后不要开 `idf.py monitor` 占用串口。直接：

```powershell
.\scripts\windows\at_probe.ps1 -Port COM3
```

预期：

```text
AT
OK
+GMR:...FW=2.1.8b...
+PINS:...
+STATUS:...
+MAC:...
```

ESP32-C3 的 `AT+PINS?` 会显示 `CFG=UART0`；Classic ESP32 显示 `CFG=UART0`。

## 5. 配 MASTER ESP32

在 ESP32 板载 USB COM 上执行 `configs/esp32_master_serial_test.txt`：

```powershell
.\scripts\windows\at_send.ps1 -Port COM10 -File ..\configs\esp32_master_serial_test.txt
```

重启后查询：

```text
AT+STATUS?
AT+MAC?
```

记录 MASTER Wi-Fi STA MAC，例如 `24:6F:28:11:22:33`。

## 6. 配 SLAVE C3

编辑 `configs/esp32c3_slave_1001_serial_test.txt`，将：

```text
AT+GATEWAY="MASTER_MAC"
```

替换为真实 MASTER MAC：

```text
AT+GATEWAY="24:6F:28:11:22:33"
```

发送：

```powershell
.\scripts\windows\at_send.ps1 -Port COM3 -File ..\configs\esp32c3_slave_1001_serial_test.txt
```

## 7. ESP-NOW 测试

MASTER Debug Console 每约 5 秒应看到：

```text
+LINKRX:HELLO,SRC=1001
```

C3 输入：

```text
AT+TESTSEND="C3_TO_ESP32"
```

MASTER 应看到：

```text
+TESTRX:SRC=1001,DST=1,TEXT=C3_TO_ESP32
```

MASTER 输入：

```text
AT+TESTSEND="ESP32_TO_C3"
```

C3 应看到：

```text
+TESTRX:SRC=1,DST=4294967295,TEXT=ESP32_TO_C3
```

## 8. 切回 STM32 业务模式

两边 Debug Console 都执行：

```text
AT+TESTMODE=0
AT+SERIALMODE=FRAME
AT+SAVE
AT+APPLY
```

Business UART 在 V2.1.8b 始终固定为 FRAME，这里的 `AT+SERIALMODE=FRAME` 仅用于配置兼容与确认。

## 9. 接 STM32

### STM32 ↔ C3

```text
STM32 PA9  USART1_TX -> C3 GPIO6 RX
STM32 PA10 USART1_RX <- C3 GPIO7 TX
STM32 PB0  ESP_WAKE  -> C3 GPIO3
STM32 PB1  ESP_READY <- C3 GPIO10
GND                    <-> GND
```

### STM32 ↔ Classic ESP32

```text
STM32 PA9  USART1_TX -> ESP32 GPIO16 RX
STM32 PA10 USART1_RX <- ESP32 GPIO17 TX
STM32 PB0  ESP_WAKE  -> ESP32 GPIO25
STM32 PB1  ESP_READY <- ESP32 GPIO26
GND                    <-> GND
```

Business UART = 921600 8N1。

板载 USB Debug/AT 可继续保持连接，不会污染 STM32 业务 UART。
