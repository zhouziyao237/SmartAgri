#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "app_config.h"
#include "at_shell.h"
#include "debug_console.h"
#include "uart_bridge.h"
#include "espnow_link.h"
#include "tcp_uplink.h"
#include "power_manager.h"
#include "ota_manager.h"
#include <string.h>
#include <stdio.h>

static const char *TAG="APP";
static app_config_t cfg;
static app_config_t staged_cfg;
static volatile bool radio_ready;
static volatile bool gateway_bound;
static volatile bool discovery_task_running;
static volatile TickType_t last_master_rx_tick;

#define GATEWAY_LOST_TIMEOUT_MS 15000u

static volatile uint32_t g_local_stm32_frames;
static volatile uint16_t g_local_stm32_last_cmd;
static volatile uint32_t g_local_stm32_last_src;

#pragma pack(push,1)
typedef struct{uint8_t role;uint8_t reserved[3];uint32_t device_id;uint16_t network_id;uint16_t group_id;uint32_t generation;} esp_node_info_t;
typedef struct{uint16_t command;int16_t result;uint32_t detail;} esp_control_result_t;
#pragma pack(pop)

typedef bool (*route_send_fn_t)(const sa_route_packet_t *r);

static uint16_t local_target(void)
{
#if CONFIG_IDF_TARGET_ESP32C3
    return SA_OTA_TARGET_ESP32C3;
#else
    return SA_OTA_TARGET_ESP32;
#endif
}

static uint16_t local_caps(void)
{
    return (uint16_t)(SA_CAP_RADIO_CONFIG|SA_CAP_AUTO_DISCOVERY|SA_CAP_CHANNEL_SCAN|
                      SA_CAP_OTA|SA_CAP_LEGACY_PROTO2);
}

static bool consume_local_stm32_route(const sa_route_packet_t *r)
{
    if(!r || r->h.dst_device != SA_DEVICE_LOCAL_ESP) return false;
    g_local_stm32_frames++;
    g_local_stm32_last_cmd = r->h.inner_cmd;
    g_local_stm32_last_src = r->h.src_device;
    return true;
}

static void print_test_rx(const sa_route_packet_t *r)
{
    char text[SA_MAX_ROUTE_PAYLOAD + 1];
    uint16_t n = r->h.payload_len;
    if(n > SA_MAX_ROUTE_PAYLOAD) n = SA_MAX_ROUTE_PAYLOAD;
    for(uint16_t i=0;i<n;i++) {
        uint8_t c = r->payload[i];
        text[i] = (c >= 32 && c <= 126) ? (char)c : '.';
    }
    text[n] = 0;
    char line[620];
    snprintf(line,sizeof(line),"+TESTRX:SRC=%lu,DST=%lu,TEXT=%s\r\n",
             (unsigned long)r->h.src_device,(unsigned long)r->h.dst_device,text);
    debug_console_write_text(line);
}

static bool uart_send_packet(const sa_route_packet_t *r)
{
    if(!r)return false;
    uint16_t l=(uint16_t)(sizeof(r->h)+r->h.payload_len);
    return uart_bridge_send_route((const uint8_t*)&r->h,l);
}

static bool send_inner(route_send_fn_t send,uint32_t src,uint32_t dst,uint16_t group,uint16_t cmd,const void *payload,uint16_t len)
{
    if(!send || len>SA_MAX_ROUTE_PAYLOAD)return false;
    uint8_t rb[SA_MAX_PAYLOAD];uint16_t rl=0;
    if(!sa_route_pack(src,dst,group,cmd,0,1,payload,len,rb,&rl))return false;
    sa_route_packet_t out;
    if(!sa_route_unpack(rb,rl,&out))return false;
    return send(&out);
}

static void fill_link_info(sa_link_info_t *x)
{
    memset(x,0,sizeof(*x));
    x->magic=SA_LINK_INFO_MAGIC;
    x->fw_version=SA_FW_VERSION_CURRENT;
    x->capabilities=local_caps();
    x->target=local_target();
    x->proto_min=SA_PROTO_MIN_SUPPORTED;
    x->proto_max=SA_PROTO_MAX_SUPPORTED;
    x->role=cfg.role;
    x->channel=cfg.channel;
    {
        wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
        (void)esp_wifi_get_channel(&x->channel,&sec);
    }
}

static void fill_radio_config(sa_radio_config_payload_t *x)
{
    memset(x,0,sizeof(*x));
    x->schema_version=SA_RADIO_CONFIG_SCHEMA;
    x->role=cfg.role;
    x->serial_mode=ESP_SERIAL_FRAME;
    x->device_id=cfg.device_id;
    x->network_id=cfg.network_id;
    x->group_id=cfg.group_id;
    x->channel=cfg.channel;
    {
        wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
        (void)esp_wifi_get_channel(&x->channel,&sec);
    }
    x->power_mode=cfg.power_mode;
    x->uplink=cfg.uplink;
    x->auto_discovery=cfg.auto_discovery;
    x->channel_scan=cfg.channel_scan;
    x->gateway_valid=cfg.gateway_valid;
    memcpy(x->gateway_mac,cfg.gateway_mac,6);
    x->fw_version=SA_FW_VERSION_CURRENT;
    x->proto_min=SA_PROTO_MIN_SUPPORTED;
    x->proto_max=SA_PROTO_MAX_SUPPORTED;
    x->capabilities=local_caps();
}

static void delayed_restart_task(void *arg)
{
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(350));
    esp_restart();
}

/* ESP-owned configuration commands travel inside the same Protocol-2 route as
 * ordinary STM32 commands. MASTER-local requests are forwarded to its attached
 * ESP by STM32; remote requests are intercepted by the target SLAVE ESP before
 * they reach that SLAVE STM32. Thus AT is a maintenance path, not a deployment
 * requirement. */
static bool handle_radio_management(const sa_route_packet_t *r,route_send_fn_t reply)
{
    if(!r || !reply)return false;
    if(r->h.inner_cmd!=SA_CMD_RADIO_CONFIG_GET && r->h.inner_cmd!=SA_CMD_RADIO_CONFIG_SET)return false;
    if(r->h.dst_device!=cfg.device_id)return false;
    if(r->h.src_device!=SA_DEVICE_PC && r->h.src_device!=SA_DEVICE_MASTER)return true;

    if(r->h.inner_cmd==SA_CMD_RADIO_CONFIG_GET){
        sa_radio_config_payload_t out;
        fill_radio_config(&out);
        (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_RADIO_CONFIG_REPLY,&out,sizeof(out));
        return true;
    }

    esp_control_result_t result={SA_CMD_RADIO_CONFIG_SET,-1,0};
    if(r->h.payload_len!=sizeof(sa_radio_config_payload_t)){
        result.detail=1;
        (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_CONTROL_RESULT,&result,sizeof(result));
        return true;
    }
    const sa_radio_config_payload_t *in=(const sa_radio_config_payload_t*)r->payload;
    if(in->schema_version!=SA_RADIO_CONFIG_SCHEMA){
        result.detail=2;
        (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_CONTROL_RESULT,&result,sizeof(result));
        return true;
    }

    app_config_t next=cfg;
    next.role=in->role;
    next.serial_mode=ESP_SERIAL_FRAME;
    next.device_id=in->device_id;
    next.network_id=in->network_id;
    next.group_id=in->group_id;
    next.channel=in->channel;
    next.power_mode=in->power_mode;
    next.uplink=in->uplink;
    next.auto_discovery=in->auto_discovery?1u:0u;
    next.channel_scan=in->channel_scan?1u:0u;
    next.gateway_valid=in->gateway_valid?1u:0u;
    if(next.gateway_valid)memcpy(next.gateway_mac,in->gateway_mac,6);else memset(next.gateway_mac,0,6);
    if(next.role==ESP_ROLE_MASTER){next.device_id=SA_DEVICE_MASTER;next.gateway_valid=0;memset(next.gateway_mac,0,6);}

    const char *why=app_config_validate(&next);
    if(why){
        result.result=-2;result.detail=3;
        (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_CONTROL_RESULT,&result,sizeof(result));
        return true;
    }
    if(app_config_save(&next)!=ESP_OK){
        result.result=-3;result.detail=4;
        (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_CONTROL_RESULT,&result,sizeof(result));
        return true;
    }

    /* Reply while the old network/channel/device identity is still live, then
     * reboot into the new configuration. */
    result.result=0;result.detail=1;
    (void)send_inner(reply,cfg.device_id,r->h.src_device,cfg.group_id,SA_CMD_CONTROL_RESULT,&result,sizeof(result));
    debug_console_write_text("[RADIOCFG] remote config saved; reboot scheduled\r\n");
    (void)xTaskCreate(delayed_restart_task,"radio_cfg_reboot",2048,NULL,9,NULL);
    return true;
}

static void announce_radio_peer_to_pc(const sa_route_packet_t *hello)
{
    if(!hello || cfg.role!=ESP_ROLE_MASTER)return;
    esp_node_info_t ni;
    memset(&ni,0,sizeof(ni));
    ni.role=ESP_ROLE_SLAVE;
    ni.device_id=hello->h.src_device;
    ni.network_id=cfg.network_id;
    ni.group_id=hello->h.group_id;
    ni.generation=0;
    if(hello->h.payload_len==sizeof(sa_link_info_t)){
        const sa_link_info_t *li=(const sa_link_info_t*)hello->payload;
        if(li->magic==SA_LINK_INFO_MAGIC)ni.role=li->role;
    }
    (void)send_inner(uart_send_packet,hello->h.src_device,SA_DEVICE_PC,hello->h.group_id,SA_CMD_NODE_INFO,&ni,sizeof(ni));
}

static void send_extended_ack_if_supported(const sa_route_packet_t *hello)
{
    if(!hello || cfg.role!=ESP_ROLE_MASTER || hello->h.inner_cmd!=SA_CMD_LINK_HELLO)return;
    /* Zero-length V2.4.2-V2.4.6 HELLO is deliberately left to MASTER STM32,
     * which already returns the legacy zero-payload LINK_ACK. */
    if(hello->h.payload_len!=sizeof(sa_link_info_t))return;
    const sa_link_info_t *peer=(const sa_link_info_t*)hello->payload;
    if(peer->magic!=SA_LINK_INFO_MAGIC)return;
    if(peer->proto_max<SA_PROTO_MIN_SUPPORTED || peer->proto_min>SA_PROTO_MAX_SUPPORTED)return;
    sa_link_info_t me;fill_link_info(&me);
    (void)send_inner(espnow_link_send_route,cfg.device_id,hello->h.src_device,hello->h.group_id,SA_CMD_LINK_ACK,&me,sizeof(me));
}

static void bind_gateway_from_ack(const sa_route_packet_t *r)
{
    if(!r || r->h.inner_cmd!=SA_CMD_LINK_ACK || r->h.src_device!=SA_DEVICE_MASTER)return;
    if(cfg.role==ESP_ROLE_MASTER)return;

    uint8_t mac[6];
    if(!espnow_link_get_peer_mac(SA_DEVICE_MASTER,mac))return;

    /* Prefer the channel advertised by a V2.4.8/V2.4.7 extended ACK.  For a
     * legacy zero-payload ACK use the RF channel captured by recv_cb at the
     * instant the packet arrived.  Never infer the binding channel from the
     * Wi-Fi driver's *current* channel because the scan task may already have
     * advanced to the next channel before this deferred callback runs. */
    uint8_t ack_channel=0u;
    if(r->h.payload_len==sizeof(sa_link_info_t)){
        const sa_link_info_t *li=(const sa_link_info_t*)r->payload;
        if(li->magic==SA_LINK_INFO_MAGIC && li->channel>=1u && li->channel<=13u)
            ack_channel=li->channel;
    }
    if(ack_channel==0u)(void)espnow_link_get_peer_channel(SA_DEVICE_MASTER,&ack_channel);
    if(ack_channel==0u){
        wifi_second_chan_t sec=WIFI_SECOND_CHAN_NONE;
        uint8_t current=cfg.channel;
        if(esp_wifi_get_channel(&current,&sec)==ESP_OK && current>=1u && current<=13u)
            ack_channel=current;
    }

    bool changed=!cfg.gateway_valid || memcmp(cfg.gateway_mac,mac,6)!=0 ||
                 (ack_channel>=1u && ack_channel<=13u && cfg.channel!=ack_channel);
    memcpy(cfg.gateway_mac,mac,6);
    cfg.gateway_valid=1u;
    if(ack_channel>=1u && ack_channel<=13u)cfg.channel=ack_channel;

    /* Stop the scan logically first, then force the radio back onto the ACK's
     * receive channel in case the scan task advanced while ACK was queued. */
    gateway_bound=true;
    last_master_rx_tick=xTaskGetTickCount();
    if(cfg.channel>=1u && cfg.channel<=13u)
        (void)esp_wifi_set_channel(cfg.channel,WIFI_SECOND_CHAN_NONE);
    espnow_link_set_gateway(cfg.gateway_mac,true);

    if(changed){
        if(app_config_save(&cfg)==ESP_OK)staged_cfg=cfg;
        char line[160];
        snprintf(line,sizeof(line),"[DISCOVERY] bound MASTER=%02X:%02X:%02X:%02X:%02X:%02X channel=%u source=ACK_RX\r\n",
                 mac[0],mac[1],mac[2],mac[3],mac[4],mac[5],cfg.channel);
        debug_console_write_text(line);
    }
}

static void radio_to_uart(const sa_route_packet_t *r)
{
    if(!r) return;
    if(r->h.src_device==SA_DEVICE_LOCAL_ESP || r->h.dst_device==SA_DEVICE_LOCAL_ESP) return;

    /* Any valid frame from the bound MASTER is link-liveness evidence.  ACK is
     * still the only frame that creates/replaces a binding. */
    if(cfg.role!=ESP_ROLE_MASTER && r->h.src_device==SA_DEVICE_MASTER)
        last_master_rx_tick=xTaskGetTickCount();

    /* Binding accepts both extended V2.4.8 ACK and legacy zero-payload ACK. */
    bind_gateway_from_ack(r);

    if(r->h.inner_cmd==SA_CMD_LINK_HELLO && cfg.role==ESP_ROLE_MASTER){
        announce_radio_peer_to_pc(r);
        send_extended_ack_if_supported(r);
    }

    if(handle_radio_management(r,espnow_link_send_route))return;
    if(ota_manager_handle_route(r, espnow_link_send_route)) return;

    if(cfg.test_mode) {
        if(r->h.inner_cmd == SA_CMD_LINK_TEST) {
            print_test_rx(r);
        } else if(r->h.inner_cmd == SA_CMD_LINK_HELLO) {
            char line[120];
            snprintf(line,sizeof(line),"+LINKRX:HELLO,SRC=%lu,LEN=%u\r\n",(unsigned long)r->h.src_device,r->h.payload_len);
            debug_console_write_text(line);
        } else {
            char line[140];
            snprintf(line,sizeof(line),"+ROUTERX:SRC=%lu,DST=%lu,CMD=0x%04X,LEN=%u\r\n",
                     (unsigned long)r->h.src_device,(unsigned long)r->h.dst_device,
                     r->h.inner_cmd,r->h.payload_len);
            debug_console_write_text(line);
        }
        return;
    }

    if(cfg.role==ESP_ROLE_MASTER && r->h.dst_device==SA_DEVICE_PC && cfg.uplink==ESP_UPLINK_TCP)
        (void)tcp_uplink_send_route(r);
    (void)uart_send_packet(r);
}

static void from_uart(const sa_route_packet_t *r)
{
    if(!r)return;
    if(handle_radio_management(r,uart_send_packet))return;
    if(cfg.test_mode)return;
    if(consume_local_stm32_route(r)) return;
    if(ota_manager_handle_route(r, uart_send_packet)) return;
    if(!radio_ready) return;
    if(cfg.role==ESP_ROLE_MASTER) {
        if(r->h.dst_device==SA_DEVICE_PC) {
            if(cfg.uplink==ESP_UPLINK_TCP) tcp_uplink_send_route(r);
        } else {
            espnow_link_send_route(r);
        }
    } else if(cfg.role==ESP_ROLE_SLAVE || cfg.role==ESP_ROLE_UNCONFIGURED) {
        espnow_link_send_route(r);
    }
}

static bool send_test_line(const char *text)
{
    if(!radio_ready || !text || !*text || cfg.role==ESP_ROLE_UNCONFIGURED) return false;
    size_t n = strlen(text);
    if(n > SA_MAX_ROUTE_PAYLOAD) n = SA_MAX_ROUTE_PAYLOAD;
    uint32_t dst = (cfg.role==ESP_ROLE_SLAVE) ? SA_DEVICE_MASTER : SA_DEVICE_BROADCAST;
    return send_inner(espnow_link_send_route,cfg.device_id,dst,cfg.group_id,SA_CMD_LINK_TEST,text,(uint16_t)n);
}

static bool sync_stm32_identity(void)
{
    if(cfg.test_mode || cfg.role==ESP_ROLE_UNCONFIGURED) return false;
    uint8_t p[12]={0};
    p[0]=(cfg.role==ESP_ROLE_MASTER)?1:2;
    memcpy(p+4,&cfg.device_id,4);
    memcpy(p+8,&cfg.network_id,2);
    memcpy(p+10,&cfg.group_id,2);
    uint8_t rb[SA_MAX_PAYLOAD];
    uint16_t rl=0;
    if(!sa_route_pack(SA_DEVICE_LOCAL_ESP,SA_DEVICE_BROADCAST,cfg.group_id,SA_CMD_NODE_SET,0,1,p,sizeof(p),rb,&rl)) return false;
    return uart_bridge_send_route(rb,rl);
}

static void wifi_evt(void *arg,esp_event_base_t base,int32_t id,void *data)
{
    (void)arg; (void)data;
    if(base==WIFI_EVENT && id==WIFI_EVENT_STA_DISCONNECTED && cfg.role==ESP_ROLE_MASTER && cfg.uplink==ESP_UPLINK_TCP)
        esp_wifi_connect();
}

static void wifi_start(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_evt,NULL));
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t ic=WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&ic));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    if(cfg.role==ESP_ROLE_MASTER && cfg.uplink==ESP_UPLINK_TCP && cfg.wifi_ssid[0]) {
        wifi_config_t w={0};
        size_t ssid_len = strnlen(cfg.wifi_ssid, sizeof(cfg.wifi_ssid));
        size_t pass_len = strnlen(cfg.wifi_pass, sizeof(cfg.wifi_pass));
        if(ssid_len == 0 || ssid_len > sizeof(w.sta.ssid) || pass_len > sizeof(w.sta.password)) {
            ESP_LOGE(TAG,"invalid WiFi credential length ssid=%u pass=%u",(unsigned)ssid_len,(unsigned)pass_len);
            ESP_ERROR_CHECK(esp_wifi_set_channel(cfg.channel,WIFI_SECOND_CHAN_NONE));
            power_manager_apply(cfg.power_mode);
            return;
        }
        memcpy(w.sta.ssid,cfg.wifi_ssid,ssid_len);
        memcpy(w.sta.password,cfg.wifi_pass,pass_len);
        w.sta.threshold.authmode=WIFI_AUTH_OPEN;
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA,&w));
        ESP_ERROR_CHECK(esp_wifi_connect());
        ESP_LOGI(TAG,"WiFi STA connecting to %s; ESP-NOW follows AP channel",cfg.wifi_ssid);
        vTaskDelay(pdMS_TO_TICKS(2500));
    } else {
        ESP_ERROR_CHECK(esp_wifi_set_channel(cfg.channel,WIFI_SECOND_CHAN_NONE));
    }
    power_manager_apply(cfg.power_mode);
}

static void discovery_task(void *arg)
{
    (void)arg;
    if(cfg.role==ESP_ROLE_MASTER || !cfg.auto_discovery){
        discovery_task_running=false;
        vTaskDelete(NULL);return;
    }
    uint8_t ch=cfg.channel;
    for(;;){
        if(gateway_bound || cfg.gateway_valid)break;
        if(cfg.channel_scan){
            if(esp_wifi_set_channel(ch,WIFI_SECOND_CHAN_NONE)!=ESP_OK){
                vTaskDelay(pdMS_TO_TICKS(100));continue;
            }
        }else{
            (void)esp_wifi_set_channel(cfg.channel,WIFI_SECOND_CHAN_NONE);
        }
        espnow_link_send_hello();
        /* ACK processing is deferred through espnow_link's delivery queue.  A
         * 300 ms dwell gives normal traffic time to bind, while the RX channel
         * is still captured independently so correctness does not depend on
         * this timing. */
        for(int i=0;i<15 && !gateway_bound;i++)vTaskDelay(pdMS_TO_TICKS(20));
        if(gateway_bound)break;
        if(cfg.channel_scan)ch=(uint8_t)((ch%13u)+1u);
        else vTaskDelay(pdMS_TO_TICKS(700));
    }
    discovery_task_running=false;
    vTaskDelete(NULL);
}

static void start_discovery(const char *reason)
{
    if(cfg.role==ESP_ROLE_MASTER || !cfg.auto_discovery || gateway_bound || discovery_task_running)return;
    discovery_task_running=true;
    if(xTaskCreate(discovery_task,"sa_discovery",4096,NULL,8,NULL)!=pdPASS){
        discovery_task_running=false;
        debug_console_write_text("[DISCOVERY] failed to create scan task\r\n");
        return;
    }
    if(reason && *reason){
        char line[128];
        snprintf(line,sizeof(line),"[DISCOVERY] scan started: %s (start_ch=%u scan=%u)\r\n",
                 reason,cfg.channel,cfg.channel_scan);
        debug_console_write_text(line);
    }
}

static void recover_stale_gateway(void)
{
    if(cfg.role==ESP_ROLE_MASTER || !cfg.auto_discovery || !gateway_bound)return;
    uint8_t old_mac[6];memcpy(old_mac,cfg.gateway_mac,6);
    uint8_t old_ch=cfg.channel;

    gateway_bound=false;
    cfg.gateway_valid=0u;
    memset(cfg.gateway_mac,0,6);
    espnow_link_set_gateway(NULL,false);
    staged_cfg=cfg;
    (void)app_config_save(&cfg);
    last_master_rx_tick=xTaskGetTickCount();

    char line[180];
    snprintf(line,sizeof(line),"[DISCOVERY] gateway stale MASTER=%02X:%02X:%02X:%02X:%02X:%02X old_ch=%u; rescanning\r\n",
             old_mac[0],old_mac[1],old_mac[2],old_mac[3],old_mac[4],old_mac[5],old_ch);
    debug_console_write_text(line);
    start_discovery("gateway timeout");
}

static void make_provisional_id_if_needed(void)
{
    if(cfg.role!=ESP_ROLE_UNCONFIGURED)return;
    uint8_t mac[6]={0};
    if(esp_read_mac(mac,ESP_MAC_WIFI_STA)==ESP_OK){
        cfg.device_id=0x70000000u|((uint32_t)mac[3]<<16)|((uint32_t)mac[4]<<8)|mac[5];
        if(cfg.device_id==SA_DEVICE_PC||cfg.device_id==SA_DEVICE_BROADCAST||cfg.device_id==SA_DEVICE_MASTER)cfg.device_id^=0x00100000u;
    }
}

void app_main(void)
{
    esp_err_t e=nvs_flash_init();
    if(e==ESP_ERR_NVS_NO_FREE_PAGES || e==ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    esp_err_t cfg_load=app_config_load(&cfg);
    make_provisional_id_if_needed();
    staged_cfg=cfg;
    gateway_bound=cfg.gateway_valid!=0;
    discovery_task_running=false;
    last_master_rx_tick=xTaskGetTickCount();
    if(cfg_load!=ESP_OK)ESP_LOGW(TAG,"config load fallback: %s; provisional device=%lu role=%s",esp_err_to_name(cfg_load),(unsigned long)cfg.device_id,app_role_name(cfg.role));

    ESP_LOGI(TAG,"SmartAgri ESP DualTarget V2.4.8 target=%s role=%s device=%lu test=%u auto=%u scan=%u",
             uart_bridge_target_name(),app_role_name(cfg.role),(unsigned long)cfg.device_id,cfg.test_mode,cfg.auto_discovery,cfg.channel_scan);
    ESP_LOGI(TAG,"debug/config=%s 115200, business UART%d TX=%d RX=%d %d 8N1 FRAME-only",
             debug_console_transport_name(), uart_bridge_uart_num(), uart_bridge_tx_pin(),uart_bridge_rx_pin(), uart_bridge_baud());

    at_shell_init(&staged_cfg,&cfg);
    at_shell_set_test_send_cb(send_test_line);
    at_shell_set_sync_stm32_cb(sync_stm32_identity);
    debug_console_init(at_shell_handle_line);
    uart_bridge_init(from_uart);
    debug_console_write_text("\r\n[CFG] SmartAgri V2.4.8 AT console ready; normal deployment supports broadcast discovery + remote config.\r\n");

    for(int i=0;i<4;i++){
        vTaskDelay(pdMS_TO_TICKS(600));
        (void)sync_stm32_identity();
    }
    wifi_start();
    ota_manager_init(&cfg);

    /* V2.4.8 starts ESP-NOW even for UNCONFIGURED nodes. They advertise a
     * deterministic provisional ID and can be remotely configured by MASTER/PC. */
    ESP_ERROR_CHECK((esp_err_t)espnow_link_init(&cfg,radio_to_uart));
    radio_ready=true;
    if(cfg.role==ESP_ROLE_MASTER){
        if(!cfg.test_mode)tcp_uplink_init(&cfg,radio_to_uart);
    }else if(cfg.auto_discovery && !cfg.gateway_valid){
        start_discovery("boot unbound");
    }else{
        vTaskDelay(pdMS_TO_TICKS(400));
        espnow_link_send_hello();
    }

    for(;;){
        (void)sync_stm32_identity();
        if(cfg.role!=ESP_ROLE_MASTER && radio_ready){
            if(gateway_bound){
                TickType_t now=xTaskGetTickCount();
                if(cfg.auto_discovery &&
                   (TickType_t)(now-last_master_rx_tick)>pdMS_TO_TICKS(GATEWAY_LOST_TIMEOUT_MS)){
                    recover_stale_gateway();
                }else{
                    espnow_link_send_hello();
                }
            }else if(cfg.auto_discovery){
                start_discovery("unbound maintenance");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
