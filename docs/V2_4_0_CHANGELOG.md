# SmartAgri V2.4.0 Changelog

## Upper computer

- Web 本地对象模板页改为全宽横向对象列表，修复 Node/按钮因窄卡片被压成竖排的问题。
- Web 页面继续使用 `BasePage -> SmartAgriApp` 页面对象模型。
- Qt 改为 `qt_app/pages/*.py` 页面插件目录；通过 `@register_page(order)` 自动注册，MainWindow 不再需要知道具体业务页面。
- Qt 使用独立 `.venv-qt`；优先安装 `PySide6-Essentials`，失败自动尝试完整 `PySide6`。
- Qt 启动器强制使用当前 venv 的 Qt plugin 路径，降低 Windows `qwindows.dll` / 多套 Qt 冲突。
- `qt_diagnose.bat` 会输出 Python 位数、PySide6 位置、Qt plugins、qwindows.dll 状态。
- 新增“远程 OTA”页面，Node 内 STM32/ESP 两个组件分别探测、上传、显示任务进度。

## Protocol

新增内部命令：

```text
0x3400 OTA_INFO_GET
0x3401 OTA_INFO_REPLY
0x3402 OTA_PREPARE
0x3403 OTA_BEGIN
0x3404 OTA_DATA
0x3405 OTA_END
0x3406 OTA_ABORT
0x3407 OTA_STATUS
0x3408 OTA_RESULT
```

数据传输沿用现有 A5 + ROUTE + ESP-NOW，不增加第二套无线协议。

## STM32

- 新增 8 KB OTA Bootloader。
- App 链接地址从 `0x08000000` 改为 `0x08002000`。
- App 最大远程镜像 54 KB，最后 2 KB Flash 仍保留 Config A/B。
- Bootloader 同时监听：USART1 921600（远程节点经 ESP）与 USART2 115200（MASTER PC 直连）。
- OTA 支持 Begin / 分块写入 / ACK / CRC32 / 重复分块恢复 / 完成后跳转新 App。
- 中途断电时 Bootloader 本身不被擦除；如果 App 无效，会留在 Bootloader 等待重新 OTA。
- Bootloader 本身不通过远程 OTA 更新。

## ESP32 / ESP32-C3

- 新增 `ota_manager`，使用 ESP-IDF OTA API 写下一 OTA app 分区并切换 boot partition。
- Classic ESP32 与 ESP32-C3 使用同一 OTA 协议；实际 target 由目标运行时返回。
- MASTER ESP 和 MASTER STM32 共用 Device ID 时，MASTER STM32 会将 `component=ESP` 的本地 OTA Route 转交给 USART1 的附属 ESP。
- 保留现有 UART pin/profile、AT/NVS role/device/network/group 配置。

## Security boundary

V2.4.0 当前实现的是工程 OTA 传输与恢复机制，使用 CRC32 做传输完整性校验。正式产品仍应增加签名固件、授权、Secure Boot/Flash Encryption 等安全机制；不要把 CRC32 当作固件真实性验证。
