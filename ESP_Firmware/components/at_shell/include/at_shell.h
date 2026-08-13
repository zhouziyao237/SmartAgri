#pragma once
#include <stdbool.h>
#include "app_config.h"

typedef bool (*at_test_send_cb_t)(const char *text);
typedef bool (*at_sync_stm32_cb_t)(void);

/* AT edits a staged copy. Runtime/radio/STM32 identity use active_cfg until
 * AT+APPLY validates+saves+restarts. This prevents half-applied split-brain. */
void at_shell_init(app_config_t *staged_cfg,const app_config_t *active_cfg);
void at_shell_set_test_send_cb(at_test_send_cb_t cb);
void at_shell_set_sync_stm32_cb(at_sync_stm32_cb_t cb);
void at_shell_handle_line(const char *line);
