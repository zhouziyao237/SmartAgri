#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum { ESP_ROLE_UNCONFIGURED=0, ESP_ROLE_MASTER=1, ESP_ROLE_SLAVE=2 } esp_role_t;
typedef enum { ESP_SERIAL_AUTO=0, ESP_SERIAL_FRAME=1 } esp_serial_mode_t;
typedef enum { ESP_UPLINK_NONE=0, ESP_UPLINK_TCP=1 } esp_uplink_t;
typedef enum { ESP_POWER_ACTIVE=0, ESP_POWER_BALANCED=1, ESP_POWER_LOW=2 } esp_power_mode_t;

typedef struct {
 uint32_t version;
 uint8_t role;
 uint8_t serial_mode;
 uint8_t uplink;
 uint8_t power_mode;
 uint8_t test_mode;
 uint8_t auto_discovery;
 uint8_t channel_scan;
 uint8_t reserved_cfg;
 uint32_t device_id;
 uint16_t network_id;
 uint16_t group_id;
 uint8_t channel;
 uint8_t gateway_mac[6];
 uint8_t gateway_valid;
 char wifi_ssid[33];
 char wifi_pass[65];
 char tcp_host[64];
 uint16_t tcp_port;
} app_config_t;

void app_config_defaults(app_config_t *c);
esp_err_t app_config_load(app_config_t *c);
esp_err_t app_config_save(const app_config_t *c);
esp_err_t app_config_erase(void);
const char *app_role_name(uint8_t r);

/* Returns NULL when valid, otherwise a stable short reason string. */
const char *app_config_validate(const app_config_t *c);
bool app_config_equal(const app_config_t *a,const app_config_t *b);
