#include "ota_manager.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG="OTA";
static app_config_t g_cfg;
static const esp_partition_t *g_part;
static esp_ota_handle_t g_handle;
static bool g_active;
static uint32_t g_transfer;
static uint32_t g_size;
static uint32_t g_next;
static uint32_t g_crc_expect;
static uint32_t g_crc_state;
static uint8_t g_state=SA_OTA_STATE_IDLE;

#if CONFIG_IDF_TARGET_ESP32C3
#define SA_ESP_TARGET SA_OTA_TARGET_ESP32C3
#elif CONFIG_IDF_TARGET_ESP32
#define SA_ESP_TARGET SA_OTA_TARGET_ESP32
#else
#error "SmartAgri OTA supports esp32/esp32c3 targets in this project"
#endif

#define SA_ESP_FW_VERSION 0x00020408u

static uint32_t crc32_update(uint32_t c,const uint8_t*d,size_t n){for(size_t i=0;i<n;i++){c^=d[i];for(int b=0;b<8;b++)c=(c&1u)?(c>>1)^0xEDB88320u:c>>1;}return c;}

static bool emit(uint32_t dst,uint16_t cmd,const void *payload,uint16_t len,ota_route_tx_cb_t tx)
{
    uint8_t raw[SA_MAX_PAYLOAD];uint16_t rl=0;sa_route_packet_t r;
    if(!tx)return false;
    if(!sa_route_pack(g_cfg.device_id,dst,g_cfg.group_id,cmd,0,1,payload,len,raw,&rl))return false;
    if(!sa_route_unpack(raw,rl,&r))return false;
    return tx(&r);
}

static bool result(uint32_t dst,uint32_t transfer,int16_t code,uint32_t detail,ota_route_tx_cb_t tx)
{
    sa_ota_result_t r={SA_OTA_COMPONENT_ESP,g_state,code,transfer,g_next,detail};
    return emit(dst,SA_CMD_OTA_RESULT,&r,sizeof(r),tx);
}

static void abort_active(void)
{
    if(g_active){esp_ota_abort(g_handle);g_active=false;}
    g_part=NULL;g_transfer=0;g_size=0;g_next=0;g_crc_expect=0;g_crc_state=0xFFFFFFFFu;g_state=SA_OTA_STATE_IDLE;
}

static void restart_task(void *arg){(void)arg;vTaskDelay(pdMS_TO_TICKS(400));esp_restart();}

void ota_manager_init(const app_config_t *cfg)
{
    if (cfg) {
        g_cfg = *cfg;
    }
    abort_active();
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    esp_err_t e=esp_ota_mark_app_valid_cancel_rollback();
    if(e!=ESP_OK && e!=ESP_ERR_OTA_ROLLBACK_INVALID_STATE)ESP_LOGW(TAG,"mark valid: %s",esp_err_to_name(e));
#endif
}

static bool handle_info(const sa_route_packet_t *r,ota_route_tx_cb_t tx)
{
    if(r->h.payload_len!=sizeof(sa_ota_info_get_t))return false;
    const sa_ota_info_get_t*q=(const sa_ota_info_get_t*)r->payload;
    if(q->component!=SA_OTA_COMPONENT_ESP)return false;
    const esp_partition_t *p=esp_ota_get_next_update_partition(NULL);
    sa_ota_info_reply_t x={SA_OTA_COMPONENT_ESP,g_state,SA_ESP_TARGET,SA_ESP_FW_VERSION,p?(uint32_t)p->size:0,0};
    x.flags=1u; /* dual OTA slot */
#if CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE
    x.flags|=2u;
#endif
    emit(r->h.src_device,SA_CMD_OTA_INFO_REPLY,&x,sizeof(x),tx);return true;
}

bool ota_manager_handle_route(const sa_route_packet_t *r,ota_route_tx_cb_t tx)
{
    if(!r || r->h.dst_device!=g_cfg.device_id)return false;
    if(r->h.inner_cmd==SA_CMD_OTA_INFO_GET)return handle_info(r,tx);
    if(r->h.payload_len<1 || r->payload[0]!=SA_OTA_COMPONENT_ESP)return false;
    uint32_t src=r->h.src_device;

    switch(r->h.inner_cmd){
        case SA_CMD_OTA_PREPARE:{
            if(r->h.payload_len!=sizeof(sa_ota_prepare_t)){g_state=SA_OTA_STATE_ERROR;result(src,0,-1,0,tx);return true;}
            const sa_ota_prepare_t*p=(const sa_ota_prepare_t*)r->payload;
            uint32_t tid=p->transfer_id;
            if(tid==0u){g_state=SA_OTA_STATE_ERROR;result(src,0,-1,0,tx);return true;}
            abort_active();
            g_transfer=tid;g_next=0;g_state=SA_OTA_STATE_READY;
            result(src,tid,0,0,tx);return true;}
        case SA_CMD_OTA_BEGIN:{
            if(r->h.payload_len!=sizeof(sa_ota_begin_t)){g_state=SA_OTA_STATE_ERROR;result(src,0,-1,0,tx);return true;}
            const sa_ota_begin_t*b=(const sa_ota_begin_t*)r->payload;
            if(b->transfer_id==0u){g_state=SA_OTA_STATE_ERROR;result(src,0,-1,0,tx);return true;}
            if(g_state!=SA_OTA_STATE_READY || g_transfer==0u || b->transfer_id!=g_transfer){
                uint32_t expected=g_transfer;g_state=SA_OTA_STATE_ERROR;result(src,b->transfer_id,-5,expected,tx);return true;
            }
            if(b->target!=SA_ESP_TARGET){g_state=SA_OTA_STATE_ERROR;g_next=0;result(src,b->transfer_id,-2,SA_ESP_TARGET,tx);return true;}
            g_part=esp_ota_get_next_update_partition(NULL);
            if(!g_part || b->image_size==0 || b->image_size>(uint32_t)g_part->size){g_state=SA_OTA_STATE_ERROR;result(src,b->transfer_id,-3,g_part?(uint32_t)g_part->size:0,tx);return true;}
            esp_err_t e=esp_ota_begin(g_part,b->image_size,&g_handle);
            if(e!=ESP_OK){g_state=SA_OTA_STATE_ERROR;result(src,b->transfer_id,-4,(uint32_t)e,tx);return true;}
            g_active=true;g_size=b->image_size;g_next=0;g_crc_expect=b->image_crc32;g_crc_state=0xFFFFFFFFu;g_state=SA_OTA_STATE_RECEIVING;
            result(src,g_transfer,0,0,tx);return true;}
        case SA_CMD_OTA_DATA:{
            if(r->h.payload_len<sizeof(sa_ota_data_head_t)){result(src,g_transfer,-10,0,tx);return true;}
            const sa_ota_data_head_t*h=(const sa_ota_data_head_t*)r->payload;uint16_t avail=(uint16_t)(r->h.payload_len-sizeof(*h));
            if(!g_active || h->transfer_id!=g_transfer){result(src,h->transfer_id,-11,g_transfer,tx);return true;}
            if(h->data_len!=avail || h->data_len==0){result(src,g_transfer,-12,avail,tx);return true;}
            if(h->offset<g_next && h->offset+h->data_len==g_next){result(src,g_transfer,0,0,tx);return true;} /* ACK lost: already written */
            if(h->offset!=g_next || g_next+h->data_len>g_size){result(src,g_transfer,-12,g_next,tx);return true;}
            const uint8_t*d=r->payload+sizeof(*h);esp_err_t e=esp_ota_write(g_handle,d,h->data_len);
            if(e!=ESP_OK){
                if(g_active){esp_ota_abort(g_handle);g_active=false;}
                g_state=SA_OTA_STATE_ERROR;result(src,g_transfer,-13,(uint32_t)e,tx);return true;
            }
            g_crc_state=crc32_update(g_crc_state,d,h->data_len);g_next+=h->data_len;result(src,g_transfer,0,0,tx);return true;}
        case SA_CMD_OTA_END:{
            if(r->h.payload_len!=sizeof(sa_ota_end_t)){result(src,g_transfer,-20,0,tx);return true;}
            const sa_ota_end_t*e=(const sa_ota_end_t*)r->payload;
            if(!g_active || e->transfer_id!=g_transfer || g_next!=g_size){result(src,e->transfer_id,-21,g_next,tx);return true;}
            g_state=SA_OTA_STATE_VERIFYING;uint32_t crc=~g_crc_state;
            if(crc!=g_crc_expect || e->image_crc32!=g_crc_expect){
                if(g_active){esp_ota_abort(g_handle);g_active=false;}
                g_state=SA_OTA_STATE_ERROR;result(src,g_transfer,-22,crc,tx);return true;
            }
            esp_err_t er=esp_ota_end(g_handle);g_active=false;
            if(er!=ESP_OK){g_state=SA_OTA_STATE_ERROR;result(src,g_transfer,-23,(uint32_t)er,tx);return true;}
            er=esp_ota_set_boot_partition(g_part);
            if(er!=ESP_OK){g_state=SA_OTA_STATE_ERROR;result(src,g_transfer,-24,(uint32_t)er,tx);return true;}
            g_state=SA_OTA_STATE_REBOOT_PENDING;result(src,g_transfer,0,crc,tx);xTaskCreate(restart_task,"ota_restart",2048,NULL,5,NULL);return true;}
        case SA_CMD_OTA_ABORT:{
            uint32_t tid=g_transfer;
            if(r->h.payload_len==sizeof(sa_ota_simple_t))tid=((const sa_ota_simple_t*)r->payload)->transfer_id;
            abort_active();
            result(src,tid,0,0,tx);return true;}
        case SA_CMD_OTA_STATUS:{
            uint32_t tid=g_transfer;
            if(r->h.payload_len==sizeof(sa_ota_simple_t)){uint32_t q=((const sa_ota_simple_t*)r->payload)->transfer_id;if(q)tid=q;}
            result(src,tid,0,0,tx);return true;}
        default:return false;
    }
}
