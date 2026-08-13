#pragma once
#include <stdbool.h>
#include "smartagri_protocol.h"
#include "app_config.h"
typedef void (*tcp_route_cb_t)(const sa_route_packet_t *route);
void tcp_uplink_init(const app_config_t *cfg,tcp_route_cb_t cb);
bool tcp_uplink_send_route(const sa_route_packet_t *route);
bool tcp_uplink_connected(void);
