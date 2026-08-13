# V2.1.8b C3 UART0 / GPIO6-7 Fix

- ESP32-C3 Debug/Config：UART0 GPIO21 TX / GPIO20 RX @ 115200，ESP_LOG + AT。
- ESP32-C3 Business：UART1 GPIO7 TX / GPIO6 RX @ 921600，FRAME-only。
- Classic ESP32 保持 UART0 GPIO1/3 @115200 Debug/AT + UART2 GPIO17/16 @921600 Business。
- 去除 C3 `esp_driver_usb_serial_jtag` 依赖与 USB Serial/JTAG AT RX 代码。
- `sdkconfig.defaults.esp32c3` 改为 `CONFIG_ESP_CONSOLE_UART_DEFAULT=y`。
