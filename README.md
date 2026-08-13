# SmartAgri V2.4.8 当前详细 README

> 更新时间：2026-08-13  
> 本文是 **当前实机接线、V2.4.8 ESP 固件、当前 STM32 App UART 映射、以及最新 Bootloader UART 修复** 的统一说明。  
> **本文优先级高于 V2.4.8 完整包内旧的 `WIRING_QUICK_REFERENCE.txt`、`STM32_Firmware/README.md` 以及部分旧注释。** 这些旧文档仍残留早期 `USART1=ESP / USART2=PC` 的定义，当前实际源码已经交换为 `USART1=PC / USART2=ESP`。

---

## 1. 当前版本与状态

### 1.1 固件版本

| 设备 | 当前源码/目标版本 | 说明 |
|---|---:|---|
| Classic ESP32 MASTER | V2.4.8 | 自动发现、Channel 恢复、远程配置、OTA 转发 |
| ESP32-C3 SLAVE | V2.4.8 | 自动发现、Channel 扫描、Gateway 恢复 |
| STM32F103C8 App | V2.4.7 source / `0x00020407` | MASTER / SLAVE 共用同一 App |
| STM32F103C8 Bootloader | V2.4.x OTA Bootloader，最新 UART 修正版 | 本次需要重新编译并用 ST-LINK 安装 |
| Route Protocol | 2 | ESP V2.4.8 对 V2.4.2–V2.4.7 基础 Route 保持 Protocol-2 兼容 |

当前实机已经验证到：

- MASTER ESP32 与 C3 都运行 V2.4.8；
- MASTER / SLAVE 已统一到 Channel 6；
- C3 已自动绑定 MASTER MAC；
- Web 已经能够读取 Node 1001 的 STM32 信息；
- 实机返回过 `Firmware 0x20406`、`Target STM32F103C8`、`Max image 54 KB`，说明 V2.4.8 ESP 与旧 STM32 App 的基础 OTA_INFO 协议兼容；
- 当前 OTA 卡在 App 重启进入 Bootloader 后的 `OTA_STATUS`，根因是旧 Bootloader UART 波特率角色仍与当前 App 相反；最新 `bootloader_v248_uart_fixed.c` 已修正这一点。

---

## 2. 整体系统架构

```text
PC / Web / Qt
     │
     │ USB-TTL 115200, SmartAgri A5 Binary Route
     ▼
MASTER STM32F103C8
     │
     │ USART2 PA2/PA3 @ 921600
     ▼
Classic ESP32 MASTER
     │
     │ ESP-NOW / Route Protocol 2
     ▼
ESP32-C3 SLAVE
     │
     │ UART1 GPIO7/GPIO6 @ 921600
     ▼
SLAVE STM32F103C8
     │
     ├─ GPIO / ADC / I2C / 1-Wire / DHT
     └─ USART3 RS485 / Modbus
```

### 2.1 角色原则

STM32 **不分 MASTER 固件和 SLAVE 固件**。

同一份 `smart_agri_comm.bin` 可以烧到所有 STM32F103C8 节点：

```text
MASTER STM32  ← attached ESP NODE_SET → role=MASTER, device=1
SLAVE STM32   ← attached ESP NODE_SET → role=SLAVE,  device=1001/1002/...
```

设备身份、Network、Group 等由附属 ESP 同步，而不是靠给 STM32 编译不同固件。

---

# 3. 最终 UART 定义 —— 以当前源码为准

这是当前最重要的接口定义。

## 3.1 STM32 UART

| STM32 外设 | 引脚 | 波特率 | 当前用途 |
|---|---|---:|---|
| USART1 | PA9 TX / PA10 RX | 115200 | PC / Web / Qt / Debug |
| USART2 | PA2 TX / PA3 RX | 921600 | attached ESP Business / OTA |
| USART3 | PB10 TX / PB11 RX | 921600 | RS485 / Modbus 等扩展 |

当前 STM32 App 源码实际为：

```c
MX_USART1_UART_Init() -> 115200
MX_USART2_UART_Init() -> 921600
```

Business `CommLink` 当前实际绑定：

```c
#define COMM_UART_HANDLE huart2
#define COMM_USART       USART2
#define COMM_DMA_CH      DMA1_Channel6
```

PC / Debug 当前实际使用 `huart1`。

> 注意：V2.4.8 完整包中部分旧 README、header 注释仍写着 USART1=ESP、USART2=PC，这是历史残留，不再作为接线依据。

---

# 4. 最终硬件接线

## 4.1 Classic ESP32 MASTER ↔ MASTER STM32

```text
Classic ESP32 MASTER            STM32F103C8 MASTER
-------------------------------------------------
GPIO17 UART2_TX   ----------->  PA3  USART2_RX
GPIO16 UART2_RX   <-----------  PA2  USART2_TX

GPIO25 WAKE       <-----------  PB0  ESP_WAKE
GPIO26 READY      ----------->  PB1  ESP_READY

GND               -----------  GND
```

业务串口：

```text
921600 baud
8 data bits
No parity
1 stop bit
FRAME-only
```

ESP32 的 UART0 仍然独立用于日志 / AT：

```text
UART0 GPIO1/GPIO3
115200 8N1
```

---

## 4.2 ESP32-C3 SLAVE ↔ SLAVE STM32

```text
ESP32-C3 SLAVE                 STM32F103C8 SLAVE
-------------------------------------------------
GPIO7  UART1_TX   ----------> PA3 USART2_RX
GPIO6  UART1_RX   <---------- PA2 USART2_TX

GPIO3  WAKE       <---------- PB0
GPIO10 READY      ----------> PB1

GND               ---------- GND
```

业务串口：

```text
921600 8N1
```

C3 调试 / AT：

```text
UART0 GPIO21/GPIO20 via board USB-UART
115200 8N1
```

### 4.2.1 TX/RX 方向

必须交叉：

```text
C3 GPIO7 TX  -> STM32 PA3 RX
C3 GPIO6 RX  <- STM32 PA2 TX
```

不要再接到 PA9 / PA10。

---

## 4.3 MASTER STM32 ↔ 上位机 USB-TTL

```text
STM32 MASTER                   USB-TTL
---------------------------------------
PA9  USART1_TX   -----------> RX
PA10 USART1_RX   <----------- TX
GND               ----------- GND
```

参数：

```text
115200 8N1
```

这里传输的是 **SmartAgri A5 Binary Route**，不是 ESP AT。

Web / Qt Backend 连接的是这一条 MASTER STM32 串口。

---

## 4.4 ST-LINK / SWD

```text
ST-LINK           STM32F103C8
-----------------------------
SWDIO    -------> PA13
SWCLK    -------> PA14
NRST     -------> NRST      （建议连接）
GND      -------> GND
3V3/Vref -------> 3.3V参考
```

建议 ST-LINK 只作为调试 / 首次 Bootloader 安装使用。

---

## 4.5 电源注意事项

不要用 STM32 最小系统板的小型 3.3 V LDO 同时给 ESP32 / ESP32-C3 大电流供电。

推荐：

```text
STM32：稳定 3.3 V / 5 V 板载供电
ESP：独立稳定电源
所有设备：共 GND
```

ESP Wi-Fi / ESP-NOW 发射瞬间电流较高，供电不足会出现随机重启、UART 错帧或无线不稳定。

---

# 5. ESP V2.4.8 无线配置

## 5.1 MASTER 推荐配置

通过 ESP32 板载 UART0 AT 口：

```text
AT
AT+ROLE=MASTER
AT+DEVICEID=1
AT+NETWORKID=1
AT+GROUPID=1
AT+CHANNEL=6
AT+UPLINK=NONE
AT+POWERMODE=ACTIVE
AT+SERIALMODE=FRAME
AT+TESTMODE=0
AT+SAVE
AT+APPLY
```

检查：

```text
AT+STATUS?
AT+CHANNEL?
AT+BUS?
```

典型关键字段：

```text
ROLE=MASTER
DEVICE=1
NETWORK=1
GROUP=1
CHANNEL=6
TEST=0
```

---

## 5.2 C3 SLAVE 推荐配置

```text
AT
AT+ROLE=SLAVE
AT+DEVICEID=1001
AT+NETWORKID=1
AT+GROUPID=1
AT+CHANNEL=6
AT+GATEWAY=CLEAR
AT+AUTODISCOVERY=1
AT+CHANNELSCAN=1
AT+UPLINK=NONE
AT+POWERMODE=ACTIVE
AT+SERIALMODE=FRAME
AT+TESTMODE=0
AT+SAVE
AT+APPLY
```

正常部署不要求手动填写 MASTER MAC。

V2.4.8 会：

```text
无 Gateway
  ↓
扫描 Channel 1..13
  ↓
FF:FF:FF:FF:FF:FF 真广播 LINK_HELLO
  ↓
MASTER LINK_ACK
  ↓
SLAVE 捕获 ACK 实际 RX Channel
  ↓
自动保存 MASTER MAC + Channel
```

检查：

```text
AT+STATUS?
AT+CHANNEL?
AT+DISCOVERY?
```

目标状态示例：

```text
CHANNEL=6
GATEWAY=C0:49:EF:F1:4B:18
AUTO=1
SCAN=1
TEST=0
```

---

# 6. V2.4.8 自动发现修复

V2.4.7 实机发现过以下竞态：

```text
CH6 收到 MASTER ACK
  ↓
ACK 进入 delivery queue
  ↓
扫描任务先跳到 CH7
  ↓
ACK 后处理才读取当前 Channel
  ↓
错误保存 CH7
```

V2.4.8 修复为：

1. ESP-NOW `recv_cb()` 在收包瞬间记录真实 RX channel；
2. RX channel 随 delivery queue 一起传递；
3. Gateway binding 使用 ACK 宣告 Channel 或捕获到的真实 RX channel；
4. 绑定完成后强制切回 ACK Channel；
5. Channel scan dwell 增加到约 300 ms；
6. 已绑定 Gateway 连续约 15 s 无 MASTER 有效流量时进入 Lost-Gateway Recovery；
7. 自动清除失效 runtime/NVS binding 并重新扫描；
8. 防止重复 discovery task。

因此最终目标是：

```text
MASTER 改了 Channel
  ↓
SLAVE 旧绑定失效
  ↓
自动判断 Gateway stale
  ↓
重新扫描
  ↓
重新绑定
```

不应该要求工程人员再执行 `AT+GATEWAY=...`。

---

# 7. ESP Business UART 诊断

命令：

```text
AT+BUS?
```

典型字段：

```text
TX_TRY
TX_FRAMES
TX_BYTES
RX_RAW
RX_FRAMES
RX_DROPS
CRC8
CRC16
LENFAIL
FRAME_ERR
PARITY_ERR
```

## 7.1 字段方向

对于 ESP：

```text
TX_*      = ESP -> STM32
RX_RAW    = STM32 -> ESP 收到的原始字节
RX_FRAMES = STM32 -> ESP 成功解析的 A5 帧
```

### 正常本地双向通信至少要求

```text
TX_FRAMES > 0
RX_RAW    > 0
RX_FRAMES > 0
```

如果：

```text
TX_FRAMES 很多
RX_RAW=0
RX_FRAMES=0
```

优先检查 STM32 TX → ESP RX 方向：

```text
STM32 PA2 -> C3 GPIO6
STM32 PA2 -> Classic ESP32 GPIO16
```

以及 STM32 是否真的运行 USART2@921600。

---

# 8. 本地 STM32 链路快速验证

在 attached ESP UART0 输入：

```text
AT+BUS?
AT+SYNCSTM32
AT+BUS?
```

`AT+SYNCSTM32` 会向 STM32 发送 NODE_SET。

STM32 App 收到合法 NODE_SET 后应回复 NODE_INFO，因此正常情况下第二次 `AT+BUS?` 应看到：

```text
TX_FRAMES 增加
RX_RAW 增加
RX_FRAMES 增加
```

这项测试完全不依赖 Web / ESP-NOW / OTA，可用来先确认 ESP ↔ STM32 本地链路。

---

# 9. STM32 Flash Layout

STM32F103C8 64 KiB Flash 当前布局：

```text
0x08000000 ┌──────────────────────────┐
           │ Bootloader              │ 8 KB
0x08002000 ├──────────────────────────┤
           │ smart_agri_comm App     │ 54 KB OTA region
0x0800F800 ├──────────────────────────┤
           │ Config A                │ 1 KB
0x0800FC00 ├──────────────────────────┤
           │ Config B                │ 1 KB
0x08010000 └──────────────────────────┘
```

App Vector Table：

```text
0x08002000
```

App 启动后将：

```c
SCB->VTOR = 0x08002000;
```

---

# 10. 当前 STM32 Bootloader UART Bug

## 10.1 已验证现象

在重新把 ESP 接到 STM32 PA2 / PA3 后，Web 已经能读取：

```text
Node       1001
Component  STM32
Target     STM32F103C8
Firmware   0x20406
Max image  54 KB
State      0
```

这证明 App 阶段链路：

```text
Web -> MASTER STM32 -> MASTER ESP -> ESP-NOW
    -> C3 -> SLAVE STM32 App -> reply
```

已经正常。

但是点击“开始 OTA”后出现：

```text
约 3%
0 / 41764 bytes
STM32 Bootloader 未返回 OTA_STATUS
```

说明 OTA 还没有开始传 App 数据，失败点是：

```text
OTA_INFO      OK
OTA_PREPARE   OK
STM32 reset
进入 Bootloader
OTA_STATUS    timeout
```

## 10.2 根因

旧 Bootloader 的 UART BRR 仍对应早期接口：

```text
USART1 ≈ 921600
USART2 ≈ 115200
```

但当前 App / 硬件已经是：

```text
USART1 PA9/PA10 = PC 115200
USART2 PA2/PA3  = ESP 921600
```

因此：

```text
C3 GPIO7/GPIO6 @921600
        ↓
STM32 App USART2@921600      OK
        ↓ OTA_PREPARE / reset
STM32 Bootloader USART2@115200
        X
OTA_STATUS 无法解析
```

---

# 11. 最新 Bootloader 修复文件

需要替换：

```text
STM32_Firmware/bootloader/bootloader.c
```

修正版文件：

```text
bootloader_v248_uart_fixed.c
```

SHA-256：

```text
05ddb41342a68da96367b745bcb67a7f028b16fced778f260a0af2d862389b30
```

替换方式：

```text
bootloader_v248_uart_fixed.c
        ↓ rename / overwrite
STM32_Firmware/bootloader/bootloader.c
```

### 11.1 修复后的 UART contract

```c
#define BOOT_PCLK2_HZ 64000000u
#define BOOT_PCLK1_HZ 32000000u
#define BOOT_PC_BAUD   115200u
#define BOOT_ESP_BAUD  921600u
#define USART_BRR_VALUE(pclk, baud) (((pclk) + ((baud) / 2u)) / (baud))
```

修正后：

```text
USART1 PA9/PA10 = PC / local OTA @ 115200
USART2 PA2/PA3  = attached ESP / remote OTA @ 921600
```

`clock64()` 下：

```text
PCLK2 = 64 MHz
PCLK1 = 32 MHz
USART1 BRR ≈ 0x22C
USART2 BRR ≈ 0x23
```

Bootloader 同时轮询 USART1 / USART2：

```c
if(rx_ready(USART1_BASE)) ...
if(rx_ready(USART2_BASE)) ...
```

因此修复后既可以支持 MASTER 本地串口 OTA，也可以支持经 attached ESP 的远程 OTA。

---

# 12. STM32 编译

## 12.1 Windows 工具路径

当前项目默认/推荐路径：

```text
ARM GCC:
D:/GCC-ENV/Arm-GCC/11.2 2022.02/bin/arm-none-eabi-gcc.exe

Ninja:
D:/CLion 2025.3.3/bin/ninja/win/x64/ninja.exe

OpenOCD:
D:/GCC-ENV/Arm-GCC/openocd/bin/openocd.exe

OpenOCD scripts:
D:/GCC-ENV/Arm-GCC/openocd/share/openocd/scripts

STM32CubeF1:
D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6
```

## 12.2 CLion

进入：

```text
STM32_Firmware
```

CMake options 推荐：

```text
-DSTM32CUBE_F1_REPOSITORY=D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6
-DOPENOCD_EXECUTABLE=D:/GCC-ENV/Arm-GCC/openocd/bin/openocd.exe
-DOPENOCD_SCRIPTS_DIR=D:/GCC-ENV/Arm-GCC/openocd/share/openocd/scripts
```

开发阶段：

```text
SMARTAGRI_CLOCK_STRICT_HSE=OFF
```

目标：

```text
smart_agri_bootloader
smart_agri_comm
smart_agri_ota_bundle
```

本次只修 Bootloader，最低需求：

```text
Build target: smart_agri_bootloader
```

生成：

```text
cmake-build-debug/smart_agri_bootloader.elf
cmake-build-debug/smart_agri_bootloader.hex
cmake-build-debug/smart_agri_bootloader.bin
```

如果也构建 App：

```text
cmake-build-debug/smart_agri_comm.elf
cmake-build-debug/smart_agri_comm.hex
cmake-build-debug/smart_agri_comm.bin
```

---

# 13. 本次应该如何刷 STM32

## 13.1 推荐：只重新刷修正后的 Bootloader

由于 App 当前已经能正常响应 OTA_INFO，本次不需要为了 UART Bug 修改 App。

用项目脚本：

```powershell
cd STM32_Firmware
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force

.\scripts\windows\flash_openocd.ps1 `
    -Elf .\cmake-build-debug\smart_agri_bootloader.elf
```

若 ST-LINK 没有接 NRST：

```powershell
.\scripts\windows\flash_openocd.ps1 `
    -Elf .\cmake-build-debug\smart_agri_bootloader.elf `
    -NoNrst
```

此脚本使用 ELF 的链接地址，因此 Bootloader 会写到：

```text
0x08000000
```

## 13.2 如果使用 STM32CubeProgrammer / ST-LINK Utility

选择：

```text
smart_agri_bootloader.bin
```

地址：

```text
0x08000000
```

不要把 `smart_agri_comm.bin` 烧到 `0x08000000`。

App 的地址必须是：

```text
smart_agri_comm.bin @ 0x08002000
```

## 13.3 如果进行了 Mass Erase

必须重新刷两部分：

```text
0x08000000 -> smart_agri_bootloader.bin
0x08002000 -> smart_agri_comm.bin
```

也可以直接：

```powershell
.\scripts\windows\flash_ota_bundle.ps1 `
    -BuildDir .\cmake-build-debug
```

该脚本会安装：

```text
smart_agri_bootloader.elf
smart_agri_comm.elf
```

---

# 14. ESP32 / C3 编译与刷写

ESP 固件目录：

```text
ESP_Firmware
```

PowerShell：

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
Get-ChildItem .\scripts\windows\*.ps1 | Unblock-File

.\scripts\windows\preflight.ps1
```

Classic ESP32 MASTER：

```powershell
.\scripts\windows\build_flash_esp32.ps1 -Port <ESP32_DEBUG_COM>
```

ESP32-C3：

```powershell
.\scripts\windows\build_flash_c3.ps1 -Port <C3_DEBUG_COM>
```

`<...COM>` 是 ESP 板自己的 UART0 USB 串口，不是 MASTER STM32 上位机 TTL COM。

刷写后检查：

```text
SmartAgri ESP DualTarget V2.4.8 target=ESP32
```

或：

```text
SmartAgri ESP DualTarget V2.4.8 target=ESP32-C3
```

---

# 15. Web / Qt 上位机

PC Tools：

```text
PC_Tools
```

可以使用：

```text
START.bat
START_WEB.bat
START_QT.bat
START_BOTH.bat
```

Backend 连接的是：

```text
MASTER STM32 USART1 PA9/PA10 @115200
```

不是 ESP UART0 AT 口。

---

# 16. STM32 远程 OTA 正确流程

目标 Node 1001 示例：

```text
Node ID   = 1001
Component = STM32 MCU
```

## 16.1 第一步：读取目标信息

点击：

```text
读取目标信息
```

正常应返回：

```text
Node
Component
Target
Firmware
Max image
State
```

能读到目标信息代表 App 阶段的整条网络链路正常。

## 16.2 第二步：选择 App Bin

只能选择：

```text
smart_agri_comm.bin
```

它是 relocated App：

```text
link address = 0x08002000
```

不要选择：

```text
smart_agri_bootloader.bin
*.elf
*.hex
```

当前 OTA 协议只远程更新 App，不远程覆盖 Bootloader。

## 16.3 第三步：开始 OTA

正常状态机：

```text
OTA_INFO
  ↓
PREPARE
  ↓
STM32 reset
  ↓
BOOTLOADER
  ↓
OTA_STATUS
  ↓
BEGIN
  ↓
TRANSFER
  ↓
VERIFY
  ↓
REBOOT
  ↓
DONE 100%
```

修正 Bootloader 后，应不再停在：

```text
3%
0 / xxxxx bytes
Bootloader 未返回 OTA_STATUS
```

而应该开始看到实际 bytes 持续增加。

---

# 17. OTA 首次安装原则

STM32 远程 OTA 的前提是目标 STM32 已经安装正确 Bootloader。

第一次或 Bootloader 本身有 Bug 时：

```text
ST-LINK
  ↓
安装/修复 Bootloader @0x08000000
```

之后：

```text
Web / Qt OTA
  ↓
只更新 App @0x08002000
```

Bootloader 自身不能依靠当前这个 App OTA 流程去修复自己。

---

# 18. 版本兼容策略

固件版本和 Route Protocol 版本是两个概念：

```text
Firmware V2.4.2 != Protocol 2
Firmware V2.4.8 != Protocol 2
```

当前：

```text
SA_ROUTE_VERSION = 2
```

因此基础设计目标是：

```text
V2.4.8 MASTER
    ↕ Protocol 2
V2.4.2~V2.4.7 node
```

共享命令保持基础兼容；新版本才支持的新能力允许降级，而不是因为 firmware semver 不同直接拒绝通信。

V2.4.8 对 discovery 的兼容包括：

- Extended LINK_ACK；
- legacy zero-payload LINK_ACK；
- legacy ACK 使用收包瞬间捕获到的 RX Channel 进行绑定。

---

# 19. 远程配置原则

AT 继续保留，但定位为：

```text
开发
调试
救砖
工厂维护
```

正常部署目标是：

```text
上位机
  ↓
MASTER STM32
  ↓
MASTER ESP
  ↓
ESP-NOW
  ↓
SLAVE ESP
  ↓
NVS / NODE_SET
```

配置应支持：

```text
读取
修改
校验
保存
恢复
JSON 导入/导出
```

无线配置包括：

```text
Role
Device ID
Network ID
Group ID
Channel
Gateway
Auto Discovery
Channel Scan
Power Mode
Uplink
```

STM32 配置包括 Sensor Slot、Driver、Pin、Interface、Address、采样周期、上报周期等。

---

# 20. STM32 其它接口

当前板级主要接口：

## I2C1

```text
PB6 SCL
PB7 SDA
100 kHz
```

## USART3 / RS485

```text
PB10 TX
PB11 RX
PB12 DE
```

## ADC 可用输入

```text
PA0
PA1
PA4
PA5
PA6
PA7
```

## 保留接口

```text
PA9 / PA10  -> PC UART
PA2 / PA3   -> ESP UART
PA13 / PA14 -> SWD
PB0 / PB1   -> ESP WAKE / READY
PB6 / PB7   -> I2C1
PB10/PB11/PB12 -> USART3 / RS485
```

配置传感器时不要把这些系统关键引脚重新分配为普通传感器 GPIO。

---

# 21. 常见故障定位

## 21.1 Web `OTA_INFO timeout`

先看 C3 / MASTER：

```text
AT+STATUS?
AT+CHANNEL?
AT+BUS?
```

确认：

```text
MASTER CHANNEL == C3 CHANNEL
TEST=0
C3 GATEWAY=BOUND
```

如果 ESP `RX_RAW=0`：优先查 STM32→ESP 本地 UART。

---

## 21.2 C3 `TX_FRAMES>0` 但 `RX_RAW=0`

当前正确线路：

```text
C3 GPIO7 -> STM32 PA3
C3 GPIO6 <- STM32 PA2
```

重点检查：

```text
STM32 PA2 有没有 TX 波形
PA2 是否真的接 GPIO6
是否共地
STM32 App 是否运行 USART2@921600
```

---

## 21.3 自动发现保存错 Channel

V2.4.7 已知竞态，升级 ESP 到 V2.4.8。

V2.4.8 应通过 RX Channel 捕获避免：

```text
ACK on CH6 -> wrongly save CH7
```

---

## 21.4 `OTA_INFO` 正常，但 `OTA_STATUS timeout`

如果表现为：

```text
能读取 STM32 信息
点击开始 OTA
0 bytes
Bootloader 未返回 OTA_STATUS
```

这说明 App 路由正常，但 Bootloader 阶段异常。

当前已知根因就是旧 Bootloader：

```text
USART2@115200  ← 错
```

应换成本 README 配套的修正版：

```text
USART2 PA2/PA3 @921600
```

然后 ST-LINK 重新刷 Bootloader。

---

## 21.5 3% 并不代表已写入 3% Firmware

如果 UI 同时显示：

```text
3%
0 / 41764 bytes
```

说明 UI 的 3% 是流程阶段进度，不是 Flash 已写入比例。

真正的数据传输要看：

```text
bytes > 0
```

---

# 22. 推荐当前操作顺序

当前不需要再重新刷 ESP32 / C3，也不需要再换线。

按下面顺序：

```text
1. 用 bootloader_v248_uart_fixed.c
   覆盖 STM32_Firmware/bootloader/bootloader.c

2. 重新构建 smart_agri_bootloader

3. 用 ST-LINK 把新 Bootloader 写入从机 STM32 0x08000000

4. 若没有 Mass Erase，现有 App 保留即可
   若做过 Mass Erase，同时重刷 App @0x08002000

5. 保持接线：
   C3 GPIO7 -> STM32 PA3
   C3 GPIO6 <- STM32 PA2

6. 上电后 Web 再点“读取目标信息”

7. 选择 smart_agri_comm.bin

8. 点“开始 OTA”

9. 检查 OTA_STATUS 是否通过

10. 检查 bytes 是否从 0 开始增长并最终 DONE 100%
```

---

# 23. 当前成功判据

整个链路最终应满足：

```text
[ESP discovery]
C3 -> AUTO/SCAN -> MASTER -> BOUND channel=6

[Local UART]
AT+BUS?: RX_RAW > 0, RX_FRAMES > 0

[OTA INFO]
Web 能读取 Node 1001 STM32F103C8 信息

[Bootloader]
OTA_STATUS 返回

[Transfer]
0 / N -> 持续增长 -> N / N

[Verify]
CRC / image validity PASS

[Reboot]
STM32 重新进入 App

[Final]
100% DONE
```

---

# 24. 本 README 的权威性说明

当前完整 V2.4.8 包是在多个历史版本上连续修复形成，因此仍存在一些**注释/文档没有跟随 UART 重映射更新**的情况。

当前接线和代码判断请按以下优先级：

```text
1. 当前实际 C 源码绑定的 huart / GPIO / baud
2. 本 README
3. 最新实机日志与 AT+BUS? 统计
4. 旧 README / changelog / 历史注释
```

当前最终 UART contract 固定为：

```text
STM32 USART1 PA9/PA10 = PC/Web/Debug 115200
STM32 USART2 PA2/PA3  = ESP Business/OTA 921600
```

后续代码、Bootloader、文档和新节点都应保持这个映射，不再来回交换。

