# 03 — ESP32-C3 Build / Flash / AT

## Build / Flash

```powershell
cd ESP_Firmware
.\scripts\windows\preflight.ps1
.\scripts\windows\build_flash_c3.ps1 -Port COM3
```

## V2.1.8b AT 入口

AT 不再走 GPIO6/7。ESP32-C3 使用板载 **UART0 / 板载 USB-UART** 作为 Debug/Config Console：

```text
Board USB -> PC COM
ESP_LOG + AT
terminal: 115200 8N1
```

GPIO6/7 是独立 Business UART：

```text
GPIO7 TX -> STM32 PA10 RX
GPIO6 RX <- STM32 PA9 TX
921600 8N1
A5 binary frame only
```

烧录完成后关闭 monitor，直接：

```powershell
.\scripts\windows\at_probe.ps1 -Port COM3
```

或串口助手发送 CRLF：

```text
AT
AT+GMR
AT+PINS?
AT+STATUS?
AT+MAC?
```

配置写入 NVS：

```text
AT+SAVE
```

应用并重启：

```text
AT+APPLY
```
