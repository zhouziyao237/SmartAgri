# 00 — 当前实物板接线、供电、时钟与调试

## A. 当前 STM32 板已确认

硬件参考图在：

- `docs/assets/STM32F103C8T6_min_board_hardware_specs.png`
- `docs/assets/STM32F103C8T6_min_board_pinout.png`

当前板：STM32F103C8T6 最小系统板，8 MHz 主晶振，32.768 kHz 晶振，SWD 接口，独立 NRST。

### 时钟配置

```text
8 MHz HSE crystal
      |
      v
PLL x9 = 72 MHz
      |
      +-- HCLK  = 72 MHz
      +-- APB1  = 36 MHz
      +-- APB2  = 72 MHz
      +-- ADC   = 12 MHz (APB2/6)
```

CubeMX：`RCC -> HSE -> Crystal/Ceramic Resonator`，不是 Bypass。

V2.1.7 开发默认：HSE 启动失败重试 3 次后使用 HSI/2 ×16 = 64 MHz 继续启动并保留 WARN；这只是开发容错，不代表 8 MHz HSE 硬件已恢复。

## B. 电源（非常重要）

当前板资料标明 XC6204B332MR 3.3 V LDO，供电能力约 150 mA。ESP32-C3 无线工作时不要由这个 3V3 LDO 供电。

推荐实物连接：

```text
PC/USB ---------------- STM32 Micro-USB
PC/USB/独立电源 -------- ESP32-C3 USB/5V input

STM32 GND -------------- ESP32-C3 GND
```

不要把两个不同 3.3 V 稳压输出硬并联。

## C. ST-LINK + SWD

板子底部 SWD 4 针通常是 3V3 / SWDIO / SWCLK / GND，NRST 在板侧排针上单独引出。

```text
ST-LINK              STM32 最小系统板
----------------------------------------
SWDIO       --------> PA13 / SWDIO
SWCLK       --------> PA14 / SWCLK
NRST/RST    --------> NRST
GND         --------> GND
VTref/3.3V  --------> 3V3 (参考电压)
```

推荐 5 线全部接。默认 OpenOCD：

`STM32_Firmware/openocd/stm32f103_stlink_swd.cfg`

配置使用 `transport select swd`，适配当前 OpenOCD 0.12.x/dev，不再使用旧 `hla_swd`。

## D. STM32 与 ESP32-C3

```text
STM32F103C8T6                         ESP32-C3
-----------------------------------------------------
PA9  / USART1_TX  -----------------> GPIO6 / RX
PA10 / USART1_RX  <----------------- GPIO7 / TX
PB0  / ESP_WAKE   -----------------> GPIO3
PB1  / ESP_READY  <----------------- GPIO10
GND              ------------------- GND
```

业务 UART：921600 / 8 data / no parity / 1 stop。

注意：TX 对 RX、RX 对 TX。

## D2. ESP Debug / Config Console（与 921600 业务串口独立）

ESP32-C3：板载 UART0 / 板载 USB-UART 直接连接 PC，ESP_LOG + AT；终端按 115200 使用。

Classic ESP32：板载 USB-UART / UART0 GPIO1(TX)、GPIO3(RX)，115200，ESP_LOG + AT。

V2.1.8b 的 AT 命令不再经过 GPIO6/7 或 GPIO16/17。业务 UART 固定为 921600 A5 FRAME-only，因此连接 STM32 后仍可一直保留板载 USB 做配置和看 Log。

## E. USART2 Debug Console（TX + RX）

```text
STM32 PA2 / USART2_TX -----> USB-TTL RX
STM32 PA3 / USART2_RX <----- USB-TTL TX
STM32 GND              ----- USB-TTL GND
```

115200 8N1。USB-TTL 使用 3.3 V TTL 电平。V2.1.7 已允许双向接收：PA3 通过 USART2 RX interrupt 收字符，主循环执行命令解析。可输入 `help/status/clock/config/stats/sensors/reboot`，按 Enter 发送。

## F. RS485 / Modbus

```text
STM32                MAX3485/SP3485
PB10 USART3_TX ----> DI
PB11 USART3_RX <---- RO
PB12 RS485_DE  ----> DE + /RE (并接控制)
3V3             ----> VCC
GND             ----> GND

MAX3485 A/B     <----> RS485 Sensor A/B
```

USART3 默认 9600 8N1。

## G. I2C

```text
PB6 = I2C1_SCL
PB7 = I2C1_SDA
3V3/GND -> Sensor
```

如果模块没有上拉，SCL/SDA 需要合适的上拉电阻到 3.3 V。

## H. 动态 ADC

```text
PA0 = ADC1_IN0
PA1 = ADC1_IN1
PA4 = ADC1_IN4
PA5 = ADC1_IN5
PA6 = ADC1_IN6
PA7 = ADC1_IN7
```

ADC 直接输入范围必须限制在 MCU 允许范围；0-10 V、4-20 mA 传感器必须先经过对应模拟前端。

## I. BOOT

正常从 Flash 运行：

```text
BOOT0 = 0
BOOT1 = 0 (推荐)
```

Micro-USB 在这块板上用于供电/预留 USB 功能，不能直接代替 ST-LINK 给芯片 SWD 下载。

## J. Board_Init 错误码

ST-LINK Watch：`g_board_error_code`。

```text
0x0000  OK / no fatal error
0x0101  HSE/PLL oscillator init failed
0x0102  SYSCLK/AHB/APB clock switch failed
0x0201  USART1 init failed
0x0202  USART2 init failed
0x0203  USART3 init failed
0x0301  I2C1 init failed
0x0401  ADC clock config failed
0x0402  ADC1 init failed
0x0403  ADC1 calibration failed
0xFFFF  Generic fatal error
```

如果 `0x0101`：继续看 `g_board_last_hal_status`, `g_board_hse_ready`, `g_board_pll_ready`。
