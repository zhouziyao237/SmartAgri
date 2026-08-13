# SmartAgri V2.4.3 STM32 UART RX Robust Fix

This package is based on V2.4.2. ESP32 / ESP32-C3 firmware stays protocol-compatible at V2.4.2; the required fix is on STM32.

## Root cause addressed

V2.4.2 used `HAL_UARTEx_ReceiveToIdle_DMA()` with DMA normal mode and rearmed the receiver from the RxEvent callback. The rearm return code was ignored. If HAL reception start/rearm failed after a pending UART error/overrun, USART1 could remain permanently unable to feed the protocol parser while ESP `AT+BUS?` TX_FRAMES continued increasing.

V2.4.3 changes USART1 RX to continuously running circular DMA and polls DMA CNDTR in the main loop. It also adds raw byte, parser CRC, UART error, DMA restart and start-failure counters.

## What to flash

Flash BOTH STM32 boards with the same V2.4.3 bootloader + app bundle. ESP32 and ESP32-C3 may remain on V2.4.2.

## Expected diagnostics

Healthy link:
`esp_raw` increases and `esp_ok` increases.

Bytes present but bad framing/baud:
`esp_raw` increases but `crc8/crc16/fe/ne` increase.

No electrical RX:
`esp_raw=0` while ESP `AT+BUS?` TX_FRAMES increases.

DMA/HAL issue:
`dma_startfail` or `uart_err/ore` increases; V2.4.3 automatically restarts RX.
