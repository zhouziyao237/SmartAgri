#pragma once
#include <stdint.h>

/* Persist the active node identity + transfer id into STM32F1 backup registers
 * before software reset.  The OTA bootloader reads this session first, so a
 * SLAVE such as Device 1001 remains addressable after App -> Bootloader reset. */
void OtaBoot_Request(uint32_t transfer_id, uint32_t device_id, uint16_t group_id);
