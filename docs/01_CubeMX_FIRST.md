# 01 — CubeMX / STM32 最小系统板配置

目标：STM32F103C8T6，HSE 8 MHz 晶振，PLL x9 = 72 MHz。

## 固定外设

```text
SYS Debug       Serial Wire
RCC HSE         Crystal/Ceramic Resonator
HSE             8 MHz
PLL             HSE x9
SYSCLK/HCLK     72 MHz
APB1            36 MHz
APB2            72 MHz
ADC clock       12 MHz (/6)

USART1          PA9 TX / PA10 RX / 921600 / DMA1 Ch4 TX + Ch5 RX
USART2          PA2 TX / PA3 RX / 115200
USART3          PB10 TX / PB11 RX / 9600
RS485_DE        PB12 output
I2C1            PB6 SCL / PB7 SDA / 100 kHz
ESP_WAKE        PB0 output
ESP_READY       PB1 input pulldown
ADC             PA0 PA1 PA4 PA5 PA6 PA7
SWD             PA13 SWDIO / PA14 SWCLK
```

工程运行时 ADC SensorManager 使用单通道动态切换采样，因此 `board.c` 是运行时权威实现；`.ioc` 用于记录所有可用 ADC pin/外设资源。

## Drivers

本包不把你的本机 STM32Cube Repository 直接作为编译 source root。第一次 CMake 会把需要的 HAL/CMSIS **复制**到：

`boards/base/SmartAgri_F103C8/Drivers/`

如果自动准备失败：

```powershell
cd STM32_Firmware
powershell -ExecutionPolicy Bypass -File .\scripts\windows\prepare_board_local_drivers.ps1 `
  -Repo "D:\STM32Cube\Repository\STM32Cube_FW_F1_V1.8.6"
```

然后删除 `cmake-build-debug` 并 Reload CMake。
