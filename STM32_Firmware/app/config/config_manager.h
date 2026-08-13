#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sensor_types.h"

typedef enum {
    SA_CFG_OK=0,
    SA_CFG_ERR_ARG=-1,
    SA_CFG_ERR_SLOT=-2,
    SA_CFG_ERR_FLASH=-3,
    SA_CFG_ERR_CRC=-4,
    SA_CFG_ERR_SCHEMA=-5
} sa_cfg_result_t;

void ConfigManager_Init(void);
const sa_node_config_t *ConfigManager_Get(void);
const sa_node_config_t *ConfigManager_GetWorking(void);

/* Sensor/application configuration transaction. Node identity is ESP-owned and
 * is never rolled back/overwritten by a sensor transaction. */
void ConfigManager_Begin(void);
sa_cfg_result_t ConfigManager_SetSlot(const sa_sensor_slot_config_t *slot);
sa_cfg_result_t ConfigManager_DeleteSlot(uint8_t slot_id);
void ConfigManager_Rollback(void);
sa_cfg_result_t ConfigManager_Commit(void);

/* Resets sensor/application configuration while preserving the current node
 * identity (role/device/network/group). */
sa_cfg_result_t ConfigManager_FactoryReset(void);

/* Attached ESP is authoritative for node identity. This call is idempotent and
 * flash-wear safe; it also updates any open sensor transaction's identity. */
sa_cfg_result_t ConfigManager_SetNode(uint8_t role,uint32_t device_id,uint16_t network_id,uint16_t group_id);

uint32_t ConfigManager_Generation(void);
bool ConfigManager_HasPersistedConfig(void);
bool ConfigManager_IdentityValid(void);
bool ConfigManager_TransactionActive(void);
