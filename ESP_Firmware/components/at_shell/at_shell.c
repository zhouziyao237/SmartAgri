#include "at_shell.h"
#include "uart_bridge.h"
#include "debug_console.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <strings.h>
#include <errno.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static app_config_t *g;
static const app_config_t *g_active;
static at_test_send_cb_t g_test_send;
static at_sync_stm32_cb_t g_sync_stm32;

static void out(const char *s) { debug_console_write_text(s); }
static void ok(void) { out("OK\r\n"); }
static void err(void) { out("ERROR\r\n"); }

static bool copy_text_checked(char *dst, size_t dst_size, const char *src)
{
    if(!dst || dst_size == 0 || !src) return false;
    size_t n = strlen(src);
    if(n >= dst_size) return false;
    memcpy(dst, src, n + 1);
    return true;
}

static bool parse_ulong_checked(const char *s,unsigned long minv,unsigned long maxv,unsigned long *outv)
{
    if(!s || !*s || !outv) return false;
    errno=0;
    char *end=NULL;
    unsigned long v=strtoul(s,&end,0);
    if(errno!=0 || !end || *end!='\0' || v<minv || v>maxv) return false;
    *outv=v;
    return true;
}

static bool macparse(const char *s, uint8_t m[6])
{
    unsigned x[6];char tail=0;
    if(sscanf(s, "%x:%x:%x:%x:%x:%x%c", &x[0], &x[1], &x[2], &x[3], &x[4], &x[5], &tail) != 6) return false;
    for(int i = 0; i < 6; ++i) {
        if(x[i] > 0xFFu) return false;
        m[i] = (uint8_t)x[i];
    }
    return true;
}

static void unquote(char *s)
{
    size_t n = strlen(s);
    if(n >= 2 && s[0] == '"' && s[n - 1] == '"') {
        memmove(s, s + 1, n - 2);
        s[n - 2] = 0;
    }
}

static bool set_wifi_fields(char *payload)
{
    char *comma = strchr(payload, ',');
    if(!comma) return false;
    *comma = 0;
    char *ssid = payload;
    char *pass = comma + 1;
    unquote(ssid);unquote(pass);
    if(!copy_text_checked(g->wifi_ssid, sizeof(g->wifi_ssid), ssid)) {
        out("+WIFI:SSID_TOO_LONG\r\nERROR\r\n");return true;
    }
    if(!copy_text_checked(g->wifi_pass, sizeof(g->wifi_pass), pass)) {
        out("+WIFI:PASSWORD_TOO_LONG\r\nERROR\r\n");return true;
    }
    ok();return true;
}

static bool set_tcp_fields(char *payload)
{
    char *comma = strrchr(payload, ',');
    if(!comma) return false;
    *comma = 0;
    char *host = payload;
    char *port_text = comma + 1;
    unquote(host);
    unsigned long port=0;
    if(!*host || !parse_ulong_checked(port_text,1,65535,&port)) {
        out("+TCP:INVALID\r\nERROR\r\n");return true;
    }
    if(!copy_text_checked(g->tcp_host, sizeof(g->tcp_host), host)) {
        out("+TCP:HOST_TOO_LONG\r\nERROR\r\n");return true;
    }
    g->tcp_port = (uint16_t)port;
    ok();return true;
}

static void config_line(const char *tag,const app_config_t *c,bool runtime_channel)
{
    uint8_t ch=c->channel;
    if(runtime_channel){
        wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
        (void)esp_wifi_get_channel(&ch,&sec);
    }
    char gw[24]="NONE";
    if(c->gateway_valid){
        snprintf(gw,sizeof(gw),"%02X:%02X:%02X:%02X:%02X:%02X",
                 c->gateway_mac[0],c->gateway_mac[1],c->gateway_mac[2],
                 c->gateway_mac[3],c->gateway_mac[4],c->gateway_mac[5]);
    }
    char x[560];
    snprintf(x,sizeof(x),
             "+%s:TARGET=%s,ROLE=%s,DEVICE=%lu,NETWORK=%u,GROUP=%u,CHANNEL=%u,GATEWAY=%s,AUTO=%u,SCAN=%u,CFG=%s,CFG_BAUD=%d,BUS=FRAME,BUS_BAUD=%d,UPLINK=%s,POWER=%u,TEST=%u,TCP=%s:%u,DIRTY=%u\r\nOK\r\n",
             tag,uart_bridge_target_name(),app_role_name(c->role),(unsigned long)c->device_id,
             c->network_id,c->group_id,ch,gw,c->auto_discovery,c->channel_scan,debug_console_transport_name(),debug_console_baud(),
             uart_bridge_baud(),c->uplink==ESP_UPLINK_TCP?"TCP":"NONE",c->power_mode,c->test_mode,
             c->tcp_host,c->tcp_port,(g_active && g && !app_config_equal(g,g_active))?1u:0u);
    out(x);
}

void at_shell_init(app_config_t *staged_cfg,const app_config_t *active_cfg) { g=staged_cfg;g_active=active_cfg; }
void at_shell_set_test_send_cb(at_test_send_cb_t cb) { g_test_send=cb; }
void at_shell_set_sync_stm32_cb(at_sync_stm32_cb_t cb) { g_sync_stm32=cb; }

void at_shell_handle_line(const char *line)
{
    if(!g || !g_active || !line) return;

    char b[256];
    size_t input_len=strlen(line);
    if(input_len>=sizeof(b)){out("+AT:LINE_TOO_LONG\r\nERROR\r\n");return;}
    memcpy(b,line,input_len+1);
    while(*b && isspace((unsigned char)b[strlen(b)-1])) b[strlen(b)-1]=0;

    if(!strcasecmp(b,"AT")){ok();return;}

    if(!strcasecmp(b,"AT+GMR")){
        char x[200];
        snprintf(x,sizeof(x),"+GMR:PROJECT=smartagri_esp_dualtarget,FW=2.4.8,TARGET=%s,IDF=5.5.x,PROTO=2,PROTO_MIN=2,PROTO_MAX=2\r\nOK\r\n",uart_bridge_target_name());
        out(x);return;
    }
    if(!strcasecmp(b,"AT+PINS?")){
        char x[300];
        if(debug_console_tx_pin()>=0){
            snprintf(x,sizeof(x),"+PINS:TARGET=%s,CFG=%s,CFG_BAUD=%d,CFG_TX=%d,CFG_RX=%d,BUS_UART=%d,BUS_TX=%d,BUS_RX=%d,BUS_BAUD=%d,WAKE=%d,READY=%d\r\nOK\r\n",
                     uart_bridge_target_name(),debug_console_transport_name(),debug_console_baud(),debug_console_tx_pin(),debug_console_rx_pin(),
                     uart_bridge_uart_num(),uart_bridge_tx_pin(),uart_bridge_rx_pin(),uart_bridge_baud(),uart_bridge_wake_pin(),uart_bridge_ready_pin());
        }else{
            snprintf(x,sizeof(x),"+PINS:TARGET=%s,CFG=%s,CFG_BAUD=%d,BUS_UART=%d,BUS_TX=%d,BUS_RX=%d,BUS_BAUD=%d,WAKE=%d,READY=%d\r\nOK\r\n",
                     uart_bridge_target_name(),debug_console_transport_name(),debug_console_baud(),uart_bridge_uart_num(),uart_bridge_tx_pin(),uart_bridge_rx_pin(),uart_bridge_baud(),uart_bridge_wake_pin(),uart_bridge_ready_pin());
        }
        out(x);return;
    }
    if(!strcasecmp(b,"AT+STATUS?")){config_line("STATUS",g_active,true);return;}
    if(!strcasecmp(b,"AT+PENDING?")){config_line("PENDING",g,false);return;}
    if(!strcasecmp(b,"AT+CONFIGCHECK")){
        const char *why=app_config_validate(g);
        if(why){char x[96];snprintf(x,sizeof(x),"+CONFIGCHECK:INVALID,%s\r\nERROR\r\n",why);out(x);}else out("+CONFIGCHECK:VALID\r\nOK\r\n");
        return;
    }
    if(!strcasecmp(b,"AT+BUS?")){
        char x[520];
        snprintf(x,sizeof(x),
                 "+BUS:TX_TRY=%lu,TX_FRAMES=%lu,TX_BYTES=%lu,TX_WRITE_FAIL=%lu,TX_WAIT_FAIL=%lu,TX_ENC_FAIL=%lu,"
                 "RX_RAW=%lu,RX_FRAMES=%lu,RX_DROPS=%lu,CRC8=%lu,CRC16=%lu,LENFAIL=%lu,FIFO_OVF=%lu,BUF_FULL=%lu,FRAME_ERR=%lu,PARITY_ERR=%lu,"
                 "BAUD=%d,TX=%d,RX=%d,WAKE=%d,READY=%d\r\nOK\r\n",
                 (unsigned long)uart_bridge_tx_attempts(),(unsigned long)uart_bridge_tx_frames(),(unsigned long)uart_bridge_tx_bytes(),
                 (unsigned long)uart_bridge_tx_write_fail(),(unsigned long)uart_bridge_tx_wait_fail(),(unsigned long)uart_bridge_tx_encode_fail(),
                 (unsigned long)uart_bridge_rx_raw_bytes(),(unsigned long)uart_bridge_rx_frames(),(unsigned long)uart_bridge_rx_parse_drops(),
                 (unsigned long)uart_bridge_rx_crc8_fail(),(unsigned long)uart_bridge_rx_crc16_fail(),(unsigned long)uart_bridge_rx_length_fail(),
                 (unsigned long)uart_bridge_rx_fifo_ovf(),(unsigned long)uart_bridge_rx_buffer_full(),
                 (unsigned long)uart_bridge_rx_frame_err(),(unsigned long)uart_bridge_rx_parity_err(),
                 uart_bridge_baud(),uart_bridge_tx_pin(),uart_bridge_rx_pin(),uart_bridge_wake_pin(),uart_bridge_ready_pin());
        out(x);return;
    }
    if(!strcasecmp(b,"AT+SYNCSTM32")){
        if(g_sync_stm32 && g_sync_stm32()) out("+SYNCSTM32:SENT\r\nOK\r\n");
        else out("+SYNCSTM32:FAILED\r\nERROR\r\n");
        return;
    }
    if(!strcasecmp(b,"AT+CHANNEL?")){
        uint8_t ch=g_active->channel;wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;(void)esp_wifi_get_channel(&ch,&sec);
        char x[48];snprintf(x,sizeof(x),"+CHANNEL:%u\r\nOK\r\n",ch);out(x);return;
    }
    if(!strcasecmp(b,"AT+MAC?")){
        uint8_t m[6];esp_read_mac(m,ESP_MAC_WIFI_STA);char x[80];
        snprintf(x,sizeof(x),"+MAC:%02X:%02X:%02X:%02X:%02X:%02X\r\nOK\r\n",m[0],m[1],m[2],m[3],m[4],m[5]);out(x);return;
    }
    if(!strcasecmp(b,"AT+TESTMODE?")){char x[48];snprintf(x,sizeof(x),"+TESTMODE:%u\r\nOK\r\n",g_active->test_mode?1u:0u);out(x);return;}
    if(!strcasecmp(b,"AT+DISCOVERY?")){char x[96];snprintf(x,sizeof(x),"+DISCOVERY:AUTO=%u,SCAN=%u,GATEWAY=%s\r\nOK\r\n",g_active->auto_discovery,g_active->channel_scan,g_active->gateway_valid?"BOUND":"UNBOUND");out(x);return;}
    if(!strcasecmp(b,"AT+SERIALMODE?")){out("+SERIALMODE:FRAME_FIXED\r\nOK\r\n");return;}

#define EQ(p) (!strncasecmp(b,p,strlen(p)))
    if(EQ("AT+ROLE=")){
        char *s=b+8;
        if(!strcasecmp(s,"MASTER"))g->role=ESP_ROLE_MASTER;
        else if(!strcasecmp(s,"SLAVE"))g->role=ESP_ROLE_SLAVE;
        else{err();return;}ok();return;
    }
    if(EQ("AT+DEVICEID=")){
        unsigned long v=0;if(!parse_ulong_checked(b+12,1,0xFFFFFFFDul,&v)){err();return;}g->device_id=(uint32_t)v;ok();return;
    }
    if(EQ("AT+NETWORKID=")){
        unsigned long v=0;if(!parse_ulong_checked(b+13,1,65535,&v)){err();return;}g->network_id=(uint16_t)v;ok();return;
    }
    if(EQ("AT+GROUPID=")){
        unsigned long v=0;if(!parse_ulong_checked(b+11,0,65535,&v)){err();return;}g->group_id=(uint16_t)v;ok();return;
    }
    if(EQ("AT+CHANNEL=")){
        unsigned long v=0;if(!parse_ulong_checked(b+11,1,13,&v)){err();return;}g->channel=(uint8_t)v;ok();return;
    }
    if(EQ("AT+AUTODISCOVERY=")){
        unsigned long v=0;if(!parse_ulong_checked(b+17,0,1,&v)){err();return;}g->auto_discovery=(uint8_t)v;ok();return;
    }
    if(EQ("AT+CHANNELSCAN=")){
        unsigned long v=0;if(!parse_ulong_checked(b+15,0,1,&v)){err();return;}g->channel_scan=(uint8_t)v;ok();return;
    }
    if(!strcasecmp(b,"AT+GATEWAY=CLEAR")){memset(g->gateway_mac,0,sizeof(g->gateway_mac));g->gateway_valid=0;ok();return;}
    if(EQ("AT+GATEWAY=")){
        char *s=b+11;unquote(s);if(!macparse(s,g->gateway_mac)){err();return;}g->gateway_valid=1;ok();return;
    }
    if(EQ("AT+SERIALMODE=")){
        char *s=b+14;if(strcasecmp(s,"AUTO")&&strcasecmp(s,"FRAME")){err();return;}g->serial_mode=ESP_SERIAL_FRAME;out("+SERIALMODE:FRAME_FIXED\r\nOK\r\n");return;
    }
    if(EQ("AT+UPLINK=")){
        char *s=b+10;if(!strcasecmp(s,"NONE"))g->uplink=ESP_UPLINK_NONE;else if(!strcasecmp(s,"TCP"))g->uplink=ESP_UPLINK_TCP;else{err();return;}ok();return;
    }
    if(EQ("AT+POWERMODE=")){
        char *s=b+13;if(!strcasecmp(s,"ACTIVE"))g->power_mode=ESP_POWER_ACTIVE;else if(!strcasecmp(s,"BALANCED"))g->power_mode=ESP_POWER_BALANCED;else if(!strcasecmp(s,"LOW"))g->power_mode=ESP_POWER_LOW;else{err();return;}ok();return;
    }
    if(EQ("AT+TESTMODE=")){
        unsigned long v=0;if(!parse_ulong_checked(b+12,0,1,&v)){err();return;}g->test_mode=(uint8_t)v;ok();return;
    }
    if(EQ("AT+TESTSEND=")){
        char *s=b+12;unquote(s);
        if(!g_active->test_mode){out("+TESTSEND:ACTIVE_TESTMODE_REQUIRED\r\nERROR\r\n");return;}
        if(!g_test_send||!*s||!g_test_send(s)){out("+TESTSEND:FAILED\r\nERROR\r\n");return;}
        out("+TESTSEND:QUEUED\r\nOK\r\n");return;
    }
    if(EQ("AT+WIFI=")){if(!set_wifi_fields(b+8))err();return;}
    if(EQ("AT+STA=")){if(!set_wifi_fields(b+7))err();return;}
    if(EQ("AT+TCP=")){if(!set_tcp_fields(b+7))err();return;}
    if(EQ("AT+TCPSERVER=")){if(!set_tcp_fields(b+13))err();return;}

    if(!strcasecmp(b,"AT+SAVE")){
        const char *why=app_config_validate(g);
        if(why){char x[96];snprintf(x,sizeof(x),"+SAVE:INVALID,%s\r\nERROR\r\n",why);out(x);return;}
        if(app_config_save(g)==ESP_OK)out("+SAVE:OK,APPLY_REQUIRED\r\nOK\r\n");else err();
        return;
    }
    if(!strcasecmp(b,"AT+RESTORE")){
        app_config_defaults(g);
        out("+RESTORE:PENDING,USE_AT+APPLY\r\nOK\r\n");
        return;
    }
    if(!strcasecmp(b,"AT+APPLY")){
        const char *why=app_config_validate(g);
        if(why){char x[96];snprintf(x,sizeof(x),"+APPLY:INVALID,%s\r\nERROR\r\n",why);out(x);return;}
        if(app_config_save(g)!=ESP_OK){out("+APPLY:SAVE_FAILED\r\nERROR\r\n");return;}
        out("+APPLY:SAVED,RESTARTING\r\nOK\r\n");
        vTaskDelay(pdMS_TO_TICKS(100));esp_restart();return;
    }
    err();
}
