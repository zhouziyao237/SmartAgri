#include "espnow_link.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define MAGIC 0x5341u
#define CHUNK 200u
#define MAX_FRAGS 3u
#define DELIVER_QUEUE_LEN 12u

static const char*TAG="ESPNOW";
static app_config_t cfg;
static espnow_route_cb_t cb;
static uint16_t tx_msgid;
static QueueHandle_t deliver_q;
static SemaphoreHandle_t map_lock;
static volatile uint32_t deliver_q_drop;
static volatile uint32_t rx_routes;
static volatile uint32_t tx_routes;

#pragma pack(push,1)
typedef struct{uint16_t magic;uint16_t network;uint16_t msg_id;uint8_t frag_index;uint8_t frag_count;uint16_t total_len;uint16_t frag_len;uint16_t route_crc;} radio_hdr_t;
#pragma pack(pop)
typedef struct{uint32_t id;uint8_t mac[6];uint8_t channel;uint32_t seen;} peer_map_t;
static peer_map_t map[24];
typedef struct{uint8_t mac[6];uint16_t msg_id;uint16_t total_len;uint16_t route_crc;uint8_t frag_count;uint8_t got_mask;uint8_t data[SA_MAX_PAYLOAD];} reasm_t;
static reasm_t rxasm[4];
typedef struct{uint8_t mac[6];uint8_t rx_channel;sa_route_packet_t route;} delivery_t;
static uint8_t bcast[6]={0xff,0xff,0xff,0xff,0xff,0xff};

static void add_peer(const uint8_t*m)
{
    if(!m || esp_now_is_peer_exist(m))return;
    esp_now_peer_info_t p={0};
    memcpy(p.peer_addr,m,6);p.channel=0;p.ifidx=WIFI_IF_STA;p.encrypt=false;
    esp_err_t e=esp_now_add_peer(&p);
    if(e!=ESP_OK&&e!=ESP_ERR_ESPNOW_EXIST)ESP_LOGW(TAG,"add peer: %s",esp_err_to_name(e));
}

static void learn(uint32_t id,const uint8_t*m,uint8_t channel)
{
    if(!id || id==SA_DEVICE_LOCAL_ESP || id==SA_DEVICE_PC || id==SA_DEVICE_BROADCAST || !m)return;
    xSemaphoreTake(map_lock,portMAX_DELAY);
    for(int i=0;i<24;i++){
        if(map[i].id==id||map[i].id==0){map[i].id=id;memcpy(map[i].mac,m,6);if(channel>=1u&&channel<=13u)map[i].channel=channel;map[i].seen++;break;}
    }
    xSemaphoreGive(map_lock);
    add_peer(m);
}

static bool find_peer_copy(uint32_t id,uint8_t out[6])
{
    bool found=false;
    if(!out)return false;
    xSemaphoreTake(map_lock,portMAX_DELAY);
    for(int i=0;i<24;i++)if(map[i].id==id){memcpy(out,map[i].mac,6);found=true;break;}
    xSemaphoreGive(map_lock);
    return found;
}

bool espnow_link_get_peer_mac(uint32_t id,uint8_t out_mac[6])
{
    return find_peer_copy(id,out_mac);
}

bool espnow_link_get_peer_channel(uint32_t id,uint8_t *out_channel)
{
    bool found=false;
    if(!out_channel)return false;
    xSemaphoreTake(map_lock,portMAX_DELAY);
    for(int i=0;i<24;i++){
        if(map[i].id==id && map[i].channel>=1u && map[i].channel<=13u){
            *out_channel=map[i].channel;found=true;break;
        }
    }
    xSemaphoreGive(map_lock);
    return found;
}

void espnow_link_set_gateway(const uint8_t mac[6],bool valid)
{
    xSemaphoreTake(map_lock,portMAX_DELAY);
    cfg.gateway_valid=valid?1u:0u;
    if(valid&&mac)memcpy(cfg.gateway_mac,mac,6);
    else memset(cfg.gateway_mac,0,6);
    xSemaphoreGive(map_lock);
    if(valid&&mac){add_peer(mac);learn(SA_DEVICE_MASTER,mac,0u);}
}

uint32_t espnow_link_peer_count(void)
{
    uint32_t n=0;
    xSemaphoreTake(map_lock,portMAX_DELAY);
    for(int i=0;i<24;i++)if(map[i].id)n++;
    xSemaphoreGive(map_lock);
    return n;
}
uint32_t espnow_link_rx_routes(void){return rx_routes;}
uint32_t espnow_link_tx_routes(void){return tx_routes;}

static reasm_t*slot_for(const uint8_t*mac,uint16_t id)
{
    for(int i=0;i<4;i++)if(rxasm[i].msg_id==id&&!memcmp(rxasm[i].mac,mac,6))return &rxasm[i];
    for(int i=0;i<4;i++)if(rxasm[i].msg_id==0){memset(&rxasm[i],0,sizeof(rxasm[i]));memcpy(rxasm[i].mac,mac,6);rxasm[i].msg_id=id;return &rxasm[i];}
    memset(&rxasm[0],0,sizeof(rxasm[0]));memcpy(rxasm[0].mac,mac,6);rxasm[0].msg_id=id;return &rxasm[0];
}

static void queue_delivery(const uint8_t*mac,uint8_t rx_channel,const uint8_t*route,uint16_t len)
{
    delivery_t item;
    memset(&item,0,sizeof(item));
    if(!sa_route_unpack(route,len,&item.route))return;
    /* Device 0 is a physical local-UART endpoint and must never cross radio. */
    if(item.route.h.src_device==SA_DEVICE_LOCAL_ESP || item.route.h.dst_device==SA_DEVICE_LOCAL_ESP)return;

    /* A bound SLAVE accepts only its learned MASTER. Before binding, a SLAVE
     * or UNCONFIGURED node deliberately accepts Protocol-2 discovery/ACK traffic
     * from any MAC in the same network/group so zero-touch pairing can occur.
     * gateway_valid/MAC can change when an ACK binds us, so take one coherent
     * snapshot instead of racing the delivery callback against set_gateway(). */
    uint8_t role; uint16_t group; bool gateway_valid; uint8_t gateway[6]={0};
    xSemaphoreTake(map_lock,portMAX_DELAY);
    role=cfg.role; group=cfg.group_id; gateway_valid=cfg.gateway_valid!=0;
    if(gateway_valid)memcpy(gateway,cfg.gateway_mac,6);
    xSemaphoreGive(map_lock);
    if(role==ESP_ROLE_SLAVE){
        if(gateway_valid&&memcmp(mac,gateway,6)!=0)return;
        if(item.route.h.group_id!=group)return;
    }else if(role==ESP_ROLE_UNCONFIGURED){
        if(item.route.h.group_id!=group)return;
    }
    memcpy(item.mac,mac,6);
    item.rx_channel=rx_channel;
    if(xQueueSend(deliver_q,&item,0)!=pdTRUE)deliver_q_drop++;
}

static void delivery_task(void *arg)
{
    (void)arg;
    delivery_t item;
    for(;;){
        if(xQueueReceive(deliver_q,&item,portMAX_DELAY)!=pdTRUE)continue;
        /* Learn on both sides. This is essential for a new SLAVE to learn an
         * old V2.4.2-V2.4.6 MASTER from the legacy zero-payload LINK_ACK. */
        learn(item.route.h.src_device,item.mac,item.rx_channel);
        rx_routes++;
        if(cb)cb(&item.route);
    }
}

static void recv_cb(const esp_now_recv_info_t*info,const uint8_t*d,int n)
{
    if(!info||!d||n<(int)sizeof(radio_hdr_t))return;
    radio_hdr_t h;memcpy(&h,d,sizeof(h));
    if(h.magic!=MAGIC||h.network!=cfg.network_id||h.frag_count<1||h.frag_count>MAX_FRAGS||h.frag_index>=h.frag_count||
       h.total_len>SA_MAX_PAYLOAD||h.frag_len>CHUNK||n!=(int)(sizeof(h)+h.frag_len))return;
    reasm_t*r=slot_for(info->src_addr,h.msg_id);
    if(r->got_mask==0){r->total_len=h.total_len;r->route_crc=h.route_crc;r->frag_count=h.frag_count;}
    if(r->total_len!=h.total_len||r->route_crc!=h.route_crc||r->frag_count!=h.frag_count){r->msg_id=0;return;}
    uint16_t off=(uint16_t)(h.frag_index*CHUNK);
    if(off+h.frag_len>r->total_len){r->msg_id=0;return;}
    memcpy(r->data+off,d+sizeof(h),h.frag_len);
    r->got_mask|=(uint8_t)(1u<<h.frag_index);
    uint8_t want=(uint8_t)((1u<<h.frag_count)-1u);
    if(r->got_mask==want){
        uint8_t rx_channel=0u;
        /* Capture the RF channel at receive time.  Discovery scanning may move
         * to the next channel before the deferred delivery task processes ACK. */
        if(info->rx_ctrl && info->rx_ctrl->channel>=1u && info->rx_ctrl->channel<=13u)
            rx_channel=(uint8_t)info->rx_ctrl->channel;
        if(sa_crc16_ccitt(r->data,r->total_len)==r->route_crc)
            queue_delivery(info->src_addr,rx_channel,r->data,r->total_len);
        r->msg_id=0;
    }
}

int espnow_link_init(const app_config_t*c,espnow_route_cb_t f)
{
    if(!c)return -1;
    cfg=*c;cb=f;memset(map,0,sizeof(map));memset(rxasm,0,sizeof(rxasm));tx_msgid=0;deliver_q_drop=0;rx_routes=0;tx_routes=0;
    map_lock=xSemaphoreCreateMutex();deliver_q=xQueueCreate(DELIVER_QUEUE_LEN,sizeof(delivery_t));
    if(!map_lock||!deliver_q)return -2;
    if(xTaskCreate(delivery_task,"espnow_deliver",6144,NULL,10,NULL)!=pdPASS)return -3;
    esp_err_t e=esp_now_init();if(e!=ESP_OK)return (int)e;
    esp_now_register_recv_cb(recv_cb);add_peer(bcast);if(cfg.gateway_valid){add_peer(cfg.gateway_mac);learn(SA_DEVICE_MASTER,cfg.gateway_mac,0u);}return 0;
}

bool espnow_link_send_route(const sa_route_packet_t*r)
{
    if(!r)return false;
    uint16_t rl=(uint16_t)(sizeof(r->h)+r->h.payload_len);
    if(rl>SA_MAX_PAYLOAD || r->h.src_device==SA_DEVICE_LOCAL_ESP || r->h.dst_device==SA_DEVICE_LOCAL_ESP)return false;
    uint8_t dst[6];
    bool gateway_valid=false;
    uint8_t gateway[6]={0};
    xSemaphoreTake(map_lock,portMAX_DELAY);
    gateway_valid=cfg.gateway_valid!=0;
    if(gateway_valid)memcpy(gateway,cfg.gateway_mac,6);
    xSemaphoreGive(map_lock);

    if(cfg.role==ESP_ROLE_SLAVE){
        /* V2.4.8: absence of a configured gateway is no longer a dead end.
         * Routes are true ESP-NOW broadcast until LINK_ACK establishes a peer. */
        if(gateway_valid)memcpy(dst,gateway,6);else memcpy(dst,bcast,6);
    }else if(cfg.role==ESP_ROLE_UNCONFIGURED){
        memcpy(dst,bcast,6);
    }else if(!find_peer_copy(r->h.dst_device,dst))memcpy(dst,bcast,6);

    xSemaphoreTake(map_lock,portMAX_DELAY);
    uint16_t mid=++tx_msgid;if(mid==0)mid=++tx_msgid;
    xSemaphoreGive(map_lock);
    uint8_t count=(uint8_t)((rl+CHUNK-1u)/CHUNK);
    uint16_t crc=sa_crc16_ccitt((const uint8_t*)&r->h,rl);
    for(uint8_t i=0;i<count;i++){
        uint16_t off=(uint16_t)(i*CHUNK);uint16_t remain=(uint16_t)(rl-off);uint16_t fl=(remain>(uint16_t)CHUNK)?(uint16_t)CHUNK:remain;
        uint8_t b[sizeof(radio_hdr_t)+CHUNK];radio_hdr_t h={MAGIC,cfg.network_id,mid,i,count,rl,fl,crc};
        memcpy(b,&h,sizeof(h));memcpy(b+sizeof(h),(const uint8_t*)&r->h+off,fl);
        if(esp_now_send(dst,b,sizeof(h)+fl)!=ESP_OK)return false;
    }
    tx_routes++;
    return true;
}

void espnow_link_send_hello(void)
{
    sa_link_info_t info;
    memset(&info,0,sizeof(info));
    info.magic=SA_LINK_INFO_MAGIC;
    info.fw_version=SA_FW_VERSION_CURRENT;
    info.capabilities=SA_CAP_RADIO_CONFIG|SA_CAP_AUTO_DISCOVERY|SA_CAP_CHANNEL_SCAN|SA_CAP_OTA|SA_CAP_LEGACY_PROTO2;
    info.target=
#if CONFIG_IDF_TARGET_ESP32C3
        SA_OTA_TARGET_ESP32C3;
#else
        SA_OTA_TARGET_ESP32;
#endif
    info.proto_min=SA_PROTO_MIN_SUPPORTED;
    info.proto_max=SA_PROTO_MAX_SUPPORTED;
    info.role=cfg.role;
    {
        uint8_t ch=cfg.channel;wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
        if(esp_wifi_get_channel(&ch,&sec)==ESP_OK)info.channel=ch;else info.channel=cfg.channel;
    }
    uint8_t b[SA_MAX_PAYLOAD];uint16_t l=0;
    if(sa_route_pack(cfg.device_id,SA_DEVICE_MASTER,cfg.group_id,SA_CMD_LINK_HELLO,0,1,&info,sizeof(info),b,&l)){
        sa_route_packet_t r;if(sa_route_unpack(b,l,&r))(void)espnow_link_send_route(&r);
    }
}
