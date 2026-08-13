# SmartAgri STM32F103C8 V2.4.7 OTA

同一套 STM32 App 用于 MASTER / SLAVE，角色与 Device ID 继续由附属 ESP 通过 `NODE_SET` 同步。

## V2.4 Flash layout

```text
0x08000000  8 KB  smart_agri_bootloader
0x08002000  54 KB smart_agri_comm OTA App region
0x0800F800  1 KB  Config A
0x0800FC00  1 KB  Config B
```

App 的 vector table 在启动时设置 `SCB->VTOR = 0x08002000`。

## Build targets

```text
smart_agri_bootloader
smart_agri_comm
smart_agri_ota_bundle   # builds both
```

第一次安装：

```powershell
.\scripts\windows\flash_ota_bundle.ps1 -BuildDir .\cmake-build-debug
```

以后 Web/Qt OTA 上传：

```text
smart_agri_comm.bin
```

Bootloader 固定在 8 KB 保护区，本协议不远程升级 Bootloader。

## UART in Bootloader

```text
USART1 PA9/PA10  921600 -> attached ESP -> remote OTA
USART2 PA2/PA3   115200 -> PC USB-TTL -> MASTER direct OTA
```

## Existing functions retained

- USART1 ESP A5 business link
- USART2 PC/Web A5 + debug text
- I2C1 PB6/PB7
- USART3 RS485/Modbus + PB12 DE
- GPIO/ADC/1-Wire/DHT sensor slots
- A/B persistent config
- MASTER authoritative routing
