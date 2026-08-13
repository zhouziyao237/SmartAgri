# 02 — STM32 CLion / Build / ST-LINK / OpenOCD

## CLion CMake 关键路径

```text
C compiler:
D:/GCC-ENV/Arm-GCC/11.2 2022.02/bin/arm-none-eabi-gcc.exe

C++ compiler:
D:/GCC-ENV/Arm-GCC/11.2 2022.02/bin/arm-none-eabi-g++.exe

Ninja:
D:/CLion 2025.3.3/bin/ninja/win/x64/ninja.exe

OpenOCD:
D:/GCC-ENV/Arm-GCC/openocd/bin/openocd.exe

OpenOCD scripts:
D:/GCC-ENV/Arm-GCC/openocd/share/openocd/scripts
```

推荐 CMake Options 追加：

```text
-DSTM32CUBE_F1_REPOSITORY=D:/STM32Cube/Repository/STM32Cube_FW_F1_V1.8.6
-DOPENOCD_EXECUTABLE=D:/GCC-ENV/Arm-GCC/openocd/bin/openocd.exe
-DOPENOCD_SCRIPTS_DIR=D:/GCC-ENV/Arm-GCC/openocd/share/openocd/scripts
```

Build target：`smart_agri_comm`。

## 先测 SWD

NRST 已接时：

```powershell
cd STM32_Firmware
.\scripts\windows\test_stlink_swd.ps1
```

不要加 `-NoNrst`。

## Flash

```powershell
.\scripts\windows\flash_openocd.ps1 `
  -Elf ".\cmake-build-debug\smart_agri_comm.elf"
```

## GDB Debug

终端 1：

```powershell
.\scripts\windows\openocd_server.ps1
```

终端 2：

```powershell
.\scripts\windows\debug_gdb.ps1 `
  -Elf ".\cmake-build-debug\smart_agri_comm.elf" `
  -GdbExe "D:\GCC-ENV\Arm-GCC\11.2 2022.02\bin\arm-none-eabi-gdb.exe"
```

## CLion Debug 重点 Watch

```text
g_board_error_code
g_board_last_hal_status
g_board_hse_ready
g_board_pll_ready
g_board_rcc_cr
g_board_rcc_cfgr
```

正常 HSE/PLL 初始化后：`g_board_hse_ready=1`, `g_board_pll_ready=1`。
