#pragma once

#include <stdbool.h>

typedef void (*debug_console_line_cb_t)(const char *line);

/*
 * Primary debug/config console (ESP_LOG + interactive AT):
 *   ESP32-C3 : UART0 GPIO7 TX / GPIO6 RX @ 115200, normally connected to
 *              the board's USB-UART bridge.
 *   ESP32    : UART0 GPIO1 TX / GPIO3 RX @ 115200, board USB-UART bridge.
 *
 * SmartAgri STM32 business UART is separate and binary-only:
 *   ESP32-C3 : UART1 GPIO7 TX / GPIO6 RX @ 921600.
 *   ESP32    : UART2 GPIO17 TX / GPIO16 RX @ 921600.
 */
void debug_console_init(debug_console_line_cb_t line_cb);
void debug_console_write_text(const char *text);

const char *debug_console_transport_name(void);
int debug_console_baud(void);
int debug_console_tx_pin(void);
int debug_console_rx_pin(void);
