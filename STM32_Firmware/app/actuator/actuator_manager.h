#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sensor_types.h"
void ActuatorManager_Init(void);
bool ActuatorManager_Command(const sa_gpio_control_t *cmd);
void ActuatorManager_Process(void);
