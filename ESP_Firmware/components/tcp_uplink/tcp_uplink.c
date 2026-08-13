#include "tcp_uplink.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "esp_log.h"
#include <string.h>
#include <unistd.h>

static const char*TAG="TCP";
static app_config_t cfg;
static tcp_route_cb_t cb;
static int sock=-1;
static SemaphoreHandle_t mx;
static uint8_t seq;

static void close_sock_locked(void)
{
    if(sock>=0){shutdown(sock,SHUT_RDWR);close(sock);sock=-1;}
}
static void close_sock(void)
{
    if(!mx)return;
    xSemaphoreTake(mx,portMAX_DELAY);close_sock_locked();xSemaphoreGive(mx);
}

static void task(void*a)
{
    (void)a;
    sa_frame_parser_t p;sa_frame_parser_init(&p);uint8_t b[512];
    for(;;){
        if(cfg.uplink!=ESP_UPLINK_TCP||!cfg.tcp_host[0]||!cfg.tcp_port){vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        if(sock<0){
            int s=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
            if(s<0){vTaskDelay(pdMS_TO_TICKS(2000));continue;}
            struct sockaddr_in d={0};d.sin_family=AF_INET;d.sin_port=htons(cfg.tcp_port);
            if(inet_pton(AF_INET,cfg.tcp_host,&d.sin_addr)!=1||connect(s,(struct sockaddr*)&d,sizeof(d))!=0){close(s);vTaskDelay(pdMS_TO_TICKS(2000));continue;}
            xSemaphoreTake(mx,portMAX_DELAY);sock=s;xSemaphoreGive(mx);
            sa_frame_parser_init(&p); /* never carry a partial frame across TCP reconnect */
            ESP_LOGI(TAG,"connected %s:%u",cfg.tcp_host,cfg.tcp_port);
        }
        int local_sock;
        xSemaphoreTake(mx,portMAX_DELAY);local_sock=sock;xSemaphoreGive(mx);
        if(local_sock<0)continue;
        int n=recv(local_sock,b,sizeof(b),0);
        if(n<=0){close_sock();vTaskDelay(pdMS_TO_TICKS(1000));continue;}
        sa_frame_t f;bool got=sa_frame_parser_feed(&p,b,n,&f);
        do{if(got&&f.cmd_id==SA_CMD_ROUTE){sa_route_packet_t r;if(sa_route_unpack(f.payload,f.payload_len,&r)&&cb)cb(&r);}got=sa_frame_parser_feed(&p,NULL,0,&f);}while(got);
    }
}

void tcp_uplink_init(const app_config_t*c,tcp_route_cb_t f)
{
    cfg=*c;cb=f;mx=xSemaphoreCreateMutex();seq=0;configASSERT(mx!=NULL);xTaskCreate(task,"tcp_uplink",6144,NULL,9,NULL);
}
bool tcp_uplink_connected(void){return sock>=0;}
bool tcp_uplink_send_route(const sa_route_packet_t*r)
{
    if(!mx||!r)return false;
    uint16_t rl=(uint16_t)(sizeof(r->h)+r->h.payload_len);uint8_t b[SA_MAX_FRAME];
    xSemaphoreTake(mx,portMAX_DELAY);
    if(sock<0){xSemaphoreGive(mx);return false;}
    size_t n=sa_frame_encode(seq++,SA_CMD_ROUTE,(const uint8_t*)&r->h,rl,b,sizeof(b));
    if(!n){xSemaphoreGive(mx);return false;}
    size_t off=0;
    while(off<n){
        int sent=send(sock,b+off,n-off,0);
        if(sent<=0){close_sock_locked();xSemaphoreGive(mx);return false;}
        off+=(size_t)sent;
    }
    xSemaphoreGive(mx);return true;
}
