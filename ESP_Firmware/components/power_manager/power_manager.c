#include "power_manager.h"
#include "esp_wifi.h"
void power_manager_apply(uint8_t mode){if(mode==0)esp_wifi_set_ps(WIFI_PS_NONE);else esp_wifi_set_ps(WIFI_PS_MIN_MODEM);}
