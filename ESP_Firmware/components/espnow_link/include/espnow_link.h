#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "smartagri_protocol.h"
#include "app_config.h"

typedef void (*espnow_route_cb_t)(const sa_route_packet_t *route);

int espnow_link_init(const app_config_t *cfg, espnow_route_cb_t cb);
bool espnow_link_send_route(const sa_route_packet_t *route);
void espnow_link_send_hello(void);

/* Runtime peer/binding helpers used by V2.4.8 auto-discovery.  They do not
 * change NVS by themselves; the application decides when a learned binding is
 * authoritative and persists it. */
bool espnow_link_get_peer_mac(uint32_t device_id, uint8_t out_mac[6]);
bool espnow_link_get_peer_channel(uint32_t device_id, uint8_t *out_channel);
void espnow_link_set_gateway(const uint8_t mac[6], bool valid);
uint32_t espnow_link_peer_count(void);
uint32_t espnow_link_rx_routes(void);
uint32_t espnow_link_tx_routes(void);
