#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "sensor_types.h"

typedef void (*sensor_telemetry_cb_t)(const sa_telemetry_payload_t *telemetry);
void SensorManager_Init(const sa_node_config_t *cfg);
void SensorManager_Reconfigure(const sa_node_config_t *cfg);
void SensorManager_Process(void);
bool SensorManager_ValidateSlot(const sa_sensor_slot_config_t *slot, uint32_t *detail);
bool SensorManager_ValidateConfig(const sa_node_config_t *cfg, uint32_t *detail);
void SensorManager_SetTelemetryCallback(sensor_telemetry_cb_t cb);
const char *SensorManager_DriverName(uint16_t driver_id);
