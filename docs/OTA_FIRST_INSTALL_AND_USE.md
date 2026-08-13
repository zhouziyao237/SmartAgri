# V2.4.2 首次安装与 OTA 使用

## 1. 修复当前 Node 1001 STM32 OTA_BEGIN failed

两块 STM32 都重新编译 V2.4.2，然后用 ST-LINK 各执行一次：

```powershell
cd STM32_Firmware
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass -Force
.\scripts\windows\flash_ota_bundle.ps1 -BuildDir .\cmake-build-debug -NoNrst
```

如果 NRST 已接 ST-LINK，去掉 `-NoNrst`。

MASTER / SLAVE 使用完全相同的 `smart_agri_bootloader.elf + smart_agri_comm.elf`。角色继续由 attached ESP 同步。

上电后检查：

- MASTER: `role=1 device=1`
- SLAVE: `role=2 device=1001`
- 新节点首次同步后 `generation` 应大于 0。

## 2. 上位机

进入 `PC_Tools`，双击 `START.bat`。Web / Qt 共用同一个 Backend。

Remote OTA 顺序：

1. Node 1001 / STM32 MCU
2. 读取目标信息
3. 选择 `STM32_Firmware/cmake-build-debug/smart_agri_comm.bin`
4. 开始 OTA
5. 应看到：`PREPARE -> BOOTLOADER -> BEGIN -> TRANSFER -> VERIFY -> REBOOT -> DONE`

严禁给 STM32 OTA 上传 bootloader.bin / ELF / HEX。OTA 只写 Application (`0x08002000`).

## 3. ESP OTA

V2.4.0a ESP 已经可以作为 STM32 OTA 的无线转发层，所以修当前 STM32 OTA 时 ESP 不必先重刷。

若要升级 ESP 自己的 OTA 状态机到 V2.4.2：

```powershell
cd ESP_Firmware
.\scripts\windows\build_flash_c3.ps1 -Port COM3
.\scripts\windows\build_flash_esp32.ps1 -Port COM20
```

之后生成的远程 OTA 文件：

- Classic ESP32: `ESP_Firmware/dist/SmartAgri_ESP32_V2.4.2.bin`
- ESP32-C3: `ESP_Firmware/dist/SmartAgri_ESP32C3_V2.4.2.bin`

远程 OTA 时先读取目标信息，ESP32 与 ESP32-C3 的应用镜像不能混用。
