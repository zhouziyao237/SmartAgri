#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "smartagri_protocol.h"

typedef void (*uart_route_cb_t)(const sa_route_packet_t *route);

/* Dedicated STM32 business UART. Binary SmartAgri A5 frames only. */
void uart_bridge_init(uart_route_cb_t route_cb);
bool uart_bridge_send_route(const uint8_t *route, uint16_t len);

const char *uart_bridge_target_name(void);
int uart_bridge_uart_num(void);
int uart_bridge_tx_pin(void);
int uart_bridge_rx_pin(void);
int uart_bridge_wake_pin(void);
int uart_bridge_ready_pin(void);
int uart_bridge_baud(void);
uint32_t uart_bridge_tx_attempts(void);
uint32_t uart_bridge_tx_frames(void);
uint32_t uart_bridge_tx_bytes(void);
uint32_t uart_bridge_tx_write_fail(void);
uint32_t uart_bridge_tx_wait_fail(void);
uint32_t uart_bridge_tx_encode_fail(void);
uint32_t uart_bridge_rx_raw_bytes(void);
uint32_t uart_bridge_rx_frames(void);
uint32_t uart_bridge_rx_parse_drops(void);
uint32_t uart_bridge_rx_crc8_fail(void);
uint32_t uart_bridge_rx_crc16_fail(void);
uint32_t uart_bridge_rx_length_fail(void);
uint32_t uart_bridge_rx_fifo_ovf(void);
uint32_t uart_bridge_rx_buffer_full(void);
uint32_t uart_bridge_rx_frame_err(void);
uint32_t uart_bridge_rx_parity_err(void);
