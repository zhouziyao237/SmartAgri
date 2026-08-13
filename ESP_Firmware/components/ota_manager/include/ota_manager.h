#pragma once
#include <stdbool.h>
#include "smartagri_protocol.h"
#include "app_config.h"

typedef bool (*ota_route_tx_cb_t)(const sa_route_packet_t *route);
void ota_manager_init(const app_config_t *cfg);
bool ota_manager_handle_route(const sa_route_packet_t *route, ota_route_tx_cb_t reply_cb);
