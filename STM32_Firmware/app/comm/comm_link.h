#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "smartagri_protocol.h"

typedef void (*comm_route_cb_t)(const sa_route_packet_t *route);
void CommLink_Init(void);
void CommLink_Process(void);
void CommLink_SetRouteCallback(comm_route_cb_t cb);
bool CommLink_SendRoute(const sa_route_packet_t *route);
bool CommLink_SendRouteRaw(const uint8_t *route_bytes, uint16_t route_len);
uint32_t CommLink_RxFrames(void);
uint32_t CommLink_RxDrops(void);
uint32_t CommLink_RawRxBytes(void);
uint32_t CommLink_Crc8Fails(void);
uint32_t CommLink_Crc16Fails(void);
uint32_t CommLink_LengthFails(void);
uint32_t CommLink_UartErrorEvents(void);
uint32_t CommLink_UartOverruns(void);
uint32_t CommLink_UartFrameErrors(void);
uint32_t CommLink_UartNoiseErrors(void);
uint32_t CommLink_UartParityErrors(void);
uint32_t CommLink_DmaRxRestarts(void);
uint32_t CommLink_DmaRxStartFails(void);
uint32_t CommLink_DmaCndtr(void);
uint32_t CommLink_DmaLastPos(void);
void CommLink_PrintUart1Diag(void);

uint32_t CommLink_TxAttempts(void);
uint32_t CommLink_TxFrames(void);
uint32_t CommLink_TxBytes(void);
uint32_t CommLink_TxHalFails(void);
uint32_t CommLink_TxEncodeFails(void);
uint32_t CommLink_TxReadyHigh(void);
uint32_t CommLink_TxReadyLow(void);
