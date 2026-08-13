#pragma once
#include <stdint.h>

/* USART2 debug console: PA2 TX / PA3 RX, 115200 8N1.
 * RX is interrupt-driven one byte at a time; parsing happens in main context.
 */
void DebugShell_Init(void);
void DebugShell_Process(void);
void DebugShell_OnUartError(void);
uint32_t DebugShell_RxBytes(void);
uint32_t DebugShell_RxOverflow(void);
