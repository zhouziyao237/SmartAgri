#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "smartagri_protocol.h"

typedef void (*pc_route_cb_t)(const sa_route_packet_t *r);

/* Formal PC/Web uplink over STM32 USART2 (PA2 TX / PA3 RX, 115200 8N1).
 * This is NOT an AT/config channel. It carries the same A5 + ROUTE protocol
 * used elsewhere in SmartAgri. Text debug output may coexist on TX; the PC
 * parser resynchronizes on 0xA5. Incoming binary frames are intercepted before
 * the optional ASCII debug shell so they are never interpreted as commands.
 */
void PcLink_Init(void);
void PcLink_Process(void);
void PcLink_SetRouteCallback(pc_route_cb_t cb);

/* Called from the USART2 RX-complete ISR owner (debug_shell.c).
 * Returns true when the byte belongs to an A5 binary frame and therefore must
 * not be fed to the ASCII debug shell.
 */
bool PcLink_ConsumeRxByte(uint8_t b);
void PcLink_OnUartError(void);

bool PcLink_SendRouteRaw(const uint8_t *route_bytes, uint16_t route_len);
bool PcLink_SendRoute(const sa_route_packet_t *r);

uint32_t PcLink_RxFrames(void);
uint32_t PcLink_RxDrops(void);
uint32_t PcLink_TxFrames(void);
uint32_t PcLink_RawRxBytes(void);
uint32_t PcLink_Crc8Fails(void);
uint32_t PcLink_Crc16Fails(void);
uint32_t PcLink_LengthFails(void);
uint32_t PcLink_UartErrorEvents(void);
