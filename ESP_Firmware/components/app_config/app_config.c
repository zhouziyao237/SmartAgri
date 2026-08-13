#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

/* Keep this value for NVS backward compatibility: the struct layout has not
 * changed since V2.1.8. Firmware version is reported separately by AT/OTA. */
#define CFG_LAYOUT_VER 0x02010800u
#define CFG_FEATURE_MARKER 0xA7u
#define SA_DEVICE_MASTER_LOCAL 1u
#define SA_DEVICE_PC_LOCAL 0xFFFFFFFEu
#define SA_DEVICE_BROADCAST_LOCAL 0xFFFFFFFFu

_Static_assert(sizeof(app_config_t)==192u, "app_config_t NVS layout changed; bump layout version/migration");

void app_config_defaults(app_config_t*c)
{
    memset(c,0,sizeof(*c));
    c->version=CFG_LAYOUT_VER;
    c->role=ESP_ROLE_UNCONFIGURED;
    c->serial_mode=ESP_SERIAL_FRAME;
    c->uplink=ESP_UPLINK_NONE;
    c->power_mode=ESP_POWER_BALANCED;
    c->test_mode=0;
    c->auto_discovery=1;
    c->channel_scan=1;
    c->reserved_cfg=CFG_FEATURE_MARKER;
    c->device_id=1001;
    c->network_id=1;
    c->group_id=1;
    c->channel=6;
    c->tcp_port=9000;
}

const char *app_config_validate(const app_config_t *c)
{
    if(!c) return "NULL";
    if(c->version!=CFG_LAYOUT_VER) return "LAYOUT_VERSION";
    if(c->serial_mode!=ESP_SERIAL_FRAME) return "SERIAL_MODE";
    if(c->uplink!=ESP_UPLINK_NONE && c->uplink!=ESP_UPLINK_TCP) return "UPLINK";
    if(c->power_mode>ESP_POWER_LOW) return "POWER_MODE";
    if(c->test_mode>1u) return "TEST_MODE";
    if(c->auto_discovery>1u) return "AUTO_DISCOVERY";
    if(c->channel_scan>1u) return "CHANNEL_SCAN";
    if(c->gateway_valid>1u) return "GATEWAY_VALID";
    if(c->channel<1u || c->channel>13u) return "CHANNEL";
    if(c->network_id==0u) return "NETWORK_ID";
    if(!memchr(c->wifi_ssid,0,sizeof(c->wifi_ssid))) return "WIFI_SSID_TERM";
    if(!memchr(c->wifi_pass,0,sizeof(c->wifi_pass))) return "WIFI_PASS_TERM";
    if(!memchr(c->tcp_host,0,sizeof(c->tcp_host))) return "TCP_HOST_TERM";

    if(c->role==ESP_ROLE_UNCONFIGURED) return NULL;
    if(c->role!=ESP_ROLE_MASTER && c->role!=ESP_ROLE_SLAVE) return "ROLE";
    if(c->device_id==0u || c->device_id==SA_DEVICE_PC_LOCAL || c->device_id==SA_DEVICE_BROADCAST_LOCAL) return "DEVICE_ID";

    /* Protocol defines logical MASTER as device 1. Prevent split-brain configs
     * such as ROLE=MASTER,DEVICE=1001 or a SLAVE colliding with Device 1. */
    if(c->role==ESP_ROLE_MASTER && c->device_id!=SA_DEVICE_MASTER_LOCAL) return "MASTER_DEVICE_MUST_BE_1";
    if(c->role==ESP_ROLE_SLAVE && c->device_id==SA_DEVICE_MASTER_LOCAL) return "SLAVE_DEVICE_CANNOT_BE_1";

    if(c->uplink==ESP_UPLINK_TCP){
        if(c->tcp_port==0u) return "TCP_PORT";
        if(c->tcp_host[0]=='\0') return "TCP_HOST";
        if(c->wifi_ssid[0]=='\0') return "WIFI_SSID";
    }
    return NULL;
}

bool app_config_equal(const app_config_t *a,const app_config_t *b)
{
    return a && b && memcmp(a,b,sizeof(*a))==0;
}

esp_err_t app_config_load(app_config_t*c)
{
    if(!c)return ESP_ERR_INVALID_ARG;
    app_config_defaults(c);
    nvs_handle_t h;
    esp_err_t e=nvs_open("smartagri",NVS_READONLY,&h);
    if(e!=ESP_OK)return e;
    size_t n=sizeof(*c);
    e=nvs_get_blob(h,"cfg",c,&n);
    nvs_close(h);
    if(e!=ESP_OK||n!=sizeof(*c)||c->version!=CFG_LAYOUT_VER){
        app_config_defaults(c);return ESP_ERR_NVS_NOT_FOUND;
    }
    /* Stored data can come from an older firmware with weak AT validation.
     * Reject impossible values instead of booting a split-brain node. */
    /* V2.4.8 reuses the three bytes that were reserved in every V2.1.8-V2.4.6
     * NVS blob.  The struct size and CFG_LAYOUT_VER therefore stay unchanged,
     * so downgrading to an older Protocol-2 firmware still loads the same blob.
     * A feature marker lets us distinguish a legacy all-zero reserved field from
     * an intentional V2.4.8 setting where discovery/scan may be disabled. */
    if(c->reserved_cfg!=CFG_FEATURE_MARKER){
        c->auto_discovery=1u;
        c->channel_scan=1u;
        c->reserved_cfg=CFG_FEATURE_MARKER;
    }
    if(app_config_validate(c)!=NULL){
        app_config_defaults(c);return ESP_ERR_INVALID_STATE;
    }
    c->serial_mode=ESP_SERIAL_FRAME;
    return ESP_OK;
}

esp_err_t app_config_save(const app_config_t*c)
{
    if(!c)return ESP_ERR_INVALID_ARG;
    app_config_t tmp=*c;
    tmp.serial_mode=ESP_SERIAL_FRAME;
    tmp.reserved_cfg=CFG_FEATURE_MARKER;
    if(app_config_validate(&tmp)!=NULL)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e=nvs_open("smartagri",NVS_READWRITE,&h);
    if(e!=ESP_OK)return e;
    e=nvs_set_blob(h,"cfg",&tmp,sizeof(tmp));
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e;
}

esp_err_t app_config_erase(void)
{
    nvs_handle_t h;
    esp_err_t e=nvs_open("smartagri",NVS_READWRITE,&h);
    if(e!=ESP_OK)return e;
    e=nvs_erase_key(h,"cfg");
    if(e==ESP_OK)e=nvs_commit(h);
    nvs_close(h);
    return e;
}

const char*app_role_name(uint8_t r)
{
    return r==ESP_ROLE_MASTER?"MASTER":r==ESP_ROLE_SLAVE?"SLAVE":"UNCONFIGURED";
}
