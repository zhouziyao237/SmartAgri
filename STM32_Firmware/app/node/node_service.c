#include "node_service.h"
#include "smartagri_protocol.h"
#include "comm_link.h"
#include "pc_link.h"
#include "config_manager.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "board.h"
#include "debug_log.h"
#include "ota_boot.h"
#include <string.h>

#define FW_VERSION 0x0204u /* protocol family; detailed OTA fw is 0x00020407 */
#define ESP_IDENTITY_WARN_MS 3000u
#define ESP_IDENTITY_LOSS_MS 7000u

static uint32_t last_hb;
static uint8_t route_buf[SA_MAX_PAYLOAD];
static bool identity_ready;
static bool identity_from_esp;
static bool identity_wait_reported;
static uint32_t identity_wait_started;
static uint32_t last_esp_identity_ms;

#pragma pack(push,1)
typedef struct{uint32_t board_id;uint16_t fw_version;uint8_t max_slots;uint8_t pin_count;uint32_t driver_mask_lo;uint32_t driver_mask_hi;} caps_head_t;
typedef struct{uint16_t pin_id;uint32_t caps;} caps_pin_t;
typedef struct{uint8_t role;uint8_t reserved[3];uint32_t device_id;uint16_t network_id;uint16_t group_id;uint32_t generation;} node_info_t;
#pragma pack(pop)

bool NodeService_IdentityReady(void){return identity_ready;}
bool NodeService_IdentityFromEsp(void){return identity_from_esp;}
uint32_t NodeService_LastEspIdentityMs(void){return last_esp_identity_ms;}

static bool identity_valid(const sa_node_config_t *c)
{
    return c && (c->role==SA_ROLE_MASTER || c->role==SA_ROLE_SLAVE) &&
           c->device_id!=0u && c->device_id!=SA_DEVICE_PC &&
           c->device_id!=SA_DEVICE_BROADCAST && c->network_id!=0u;
}

/* V2.4.4 local-PC availability policy
 * ----------------------------------
 * The attached ESP remains the only authority that may activate radio routing.
 * However, a MASTER STM32 with a valid persisted MASTER identity must remain
 * locally manageable over USART2 even while the ESP link is down/WAIT_ESP.
 * This keeps diagnostics, local configuration and STM32 OTA reachable instead
 * of making an ESP/UART1 fault look like a dead PC port.
 */
static bool persisted_master_local_pc_available(const sa_node_config_t *c)
{
    return c && ConfigManager_HasPersistedConfig() && identity_valid(c) &&
           c->role==SA_ROLE_MASTER;
}

static bool local_pc_available(const sa_node_config_t *c)
{
    if(!c || !identity_valid(c) || c->role!=SA_ROLE_MASTER) return false;
    return identity_ready || ConfigManager_HasPersistedConfig();
}

/* Egress policy:
 *   MASTER -> PC destination : formal USART2 PC/Web link
 *   everything else          : USART1 -> attached ESP radio coprocessor
 *   SLAVE -> PC              : also goes through attached ESP, then ESP-NOW
 *                              to MASTER STM32, which terminates the PC route.
 */
static bool send_route_raw_by_dst(uint32_t dst,const uint8_t *b,uint16_t l)
{
    const sa_node_config_t *c=ConfigManager_Get();
    if(!identity_valid(c)) return false;

    /* Local PC management is intentionally independent from radio readiness.
     * A persisted MASTER may always answer the directly attached PC. */
    if(dst==SA_DEVICE_PC && local_pc_available(c)) {
        bool pc_ok = PcLink_SendRouteRaw(b,l);
        /* Mirror locally generated MASTER->PC traffic to the attached ESP when
         * identity is authoritative.  With UPLINK=TCP the ESP can forward the
         * same route to the cloud; with UPLINK=NONE it simply drops the mirror.
         * Remote PC-bound routes arriving from ESP use PcLink_SendRoute()
         * directly, so this does not create a UART/radio loop. */
        if(identity_ready) (void)CommLink_SendRouteRaw(b,l);
        return pc_ok;
    }

    /* Every egress that touches the ESP/radio still requires an authoritative
     * attached-ESP identity. No fallback identity is allowed on the air. */
    if(!identity_ready) return false;
    return CommLink_SendRouteRaw(b,l);
}

static bool send_to(uint32_t dst,uint16_t cmd,const void*p,uint16_t len)
{
    const sa_node_config_t*c=ConfigManager_Get();
    if(!identity_valid(c)) return false;
    if(!identity_ready && !(dst==SA_DEVICE_PC && local_pc_available(c))) return false;
    uint16_t rl=0;
    if(!sa_route_pack(c->device_id,dst,c->group_id,cmd,0,1,p,len,route_buf,&rl))return false;
    return send_route_raw_by_dst(dst,route_buf,rl);
}

static void send_result(uint32_t dst,uint16_t cmd,int16_t result,uint32_t detail)
{
    sa_control_result_t r={cmd,result,detail};
    send_to(dst,SA_CMD_CONTROL_RESULT,&r,sizeof(r));
}

static void send_ota_result(uint32_t dst,uint32_t transfer,uint8_t state,int16_t result,uint32_t next_offset,uint32_t detail)
{
    sa_ota_result_t r={SA_OTA_COMPONENT_STM32,state,result,transfer,next_offset,detail};
    send_to(dst,SA_CMD_OTA_RESULT,&r,sizeof(r));
}

static void send_ota_info(uint32_t dst)
{
    sa_ota_info_reply_t i={SA_OTA_COMPONENT_STM32,SA_OTA_STATE_IDLE,SA_OTA_TARGET_STM32F103C8,0x00020407u,54u*1024u,3u};
    send_to(dst,SA_CMD_OTA_INFO_REPLY,&i,sizeof(i));
}

static void send_caps(uint32_t dst)
{
    uint8_t b[220];
    caps_head_t h={0x0103C8u,FW_VERSION,SA_MAX_SENSOR_SLOTS,0,0,0};
    uint16_t pins[]={SA_PIN_PA0,SA_PIN_PA1,SA_PIN_PA4,SA_PIN_PA5,SA_PIN_PA6,SA_PIN_PA7,SA_PIN_PA8,SA_PIN_PA11,SA_PIN_PA12,SA_PIN_PB5,SA_PIN_PB8,SA_PIN_PB9,SA_PIN_PB13,SA_PIN_PB14,SA_PIN_PB15};
    h.pin_count=(uint8_t)(sizeof(pins)/sizeof(pins[0]));
    uint16_t drivers[]={SA_DRV_GPIO_INPUT,SA_DRV_ADC_RAW,SA_DRV_ADC_LINEAR,SA_DRV_4_20MA,SA_DRV_0_10V,SA_DRV_DHT11,SA_DRV_DHT22,SA_DRV_DS18B20,SA_DRV_SHT3X,SA_DRV_BME280,SA_DRV_BH1750,SA_DRV_SCD4X,SA_DRV_ADS1115,SA_DRV_I2C_GENERIC,SA_DRV_MODBUS_GENERIC,SA_DRV_PULSE_COUNTER};
    for(unsigned i=0;i<sizeof(drivers)/sizeof(drivers[0]);i++){
        uint16_t d=drivers[i];
        if(d<32)h.driver_mask_lo|=1u<<d;
        else if(d<64)h.driver_mask_hi|=1u<<(d-32);
    }
    memcpy(b,&h,sizeof(h));
    size_t off=sizeof(h);
    for(unsigned i=0;i<h.pin_count;i++){
        caps_pin_t x={pins[i],Board_PinCapabilities(pins[i])};
        memcpy(b+off,&x,sizeof(x));off+=sizeof(x);
    }
    send_to(dst,SA_CMD_CAPS_REPLY,b,(uint16_t)off);
}

static bool send_node_info(uint32_t dst)
{
    const sa_node_config_t*c=ConfigManager_Get();
    node_info_t i={c->role,{0},c->device_id,c->network_id,c->group_id,ConfigManager_Generation()};
    return send_to(dst,SA_CMD_NODE_INFO,&i,sizeof(i));
}

static void on_telemetry(const sa_telemetry_payload_t*t)
{
    send_to(SA_DEVICE_PC,SA_CMD_TELEMETRY,t,sizeof(*t));
}

static bool local_dst(const sa_route_packet_t*r)
{
    const sa_node_config_t*c=ConfigManager_Get();
    return r->h.dst_device==c->device_id||r->h.dst_device==SA_DEVICE_BROADCAST;
}

static bool local_group_match(const sa_route_packet_t *r)
{
    const sa_node_config_t *c=ConfigManager_Get();
    return r && c && r->h.group_id==c->group_id;
}

static bool ota_targets_attached_esp(const sa_route_packet_t *r)
{
    if(!r || r->h.payload_len < 1) return false;
    switch(r->h.inner_cmd){
        case SA_CMD_OTA_INFO_GET:
        case SA_CMD_OTA_PREPARE:
        case SA_CMD_OTA_BEGIN:
        case SA_CMD_OTA_DATA:
        case SA_CMD_OTA_END:
        case SA_CMD_OTA_ABORT:
        case SA_CMD_OTA_STATUS:
            return r->payload[0] == SA_OTA_COMPONENT_ESP;
        default:
            return false;
    }
}

/* ESP radio configuration is owned by the physically attached ESP, even when
 * the route destination is the logical Node ID.  MASTER-local PC requests must
 * therefore cross USART1 instead of being consumed by STM32 process_local(). */
static bool radio_targets_attached_esp(const sa_route_packet_t *r)
{
    if(!r) return false;
    return r->h.inner_cmd==SA_CMD_RADIO_CONFIG_GET ||
           r->h.inner_cmd==SA_CMD_RADIO_CONFIG_SET;
}

static bool radio_reply_from_attached_esp(const sa_route_packet_t *r)
{
    if(!r || r->h.dst_device!=SA_DEVICE_PC) return false;
    if(r->h.inner_cmd==SA_CMD_RADIO_CONFIG_REPLY) return true;
    if(r->h.inner_cmd==SA_CMD_CONTROL_RESULT && r->h.payload_len==sizeof(sa_control_result_t)){
        const sa_control_result_t *x=(const sa_control_result_t*)r->payload;
        return x->command==SA_CMD_RADIO_CONFIG_SET;
    }
    return false;
}

static bool handle_attached_esp_identity(const sa_route_packet_t *r)
{
    if(!r || r->h.inner_cmd!=SA_CMD_NODE_SET) return false;

    /* NODE_SET is a local coprocessor control message, not a network command.
     * Only src=SA_DEVICE_LOCAL_ESP arriving over USART1 may change identity.
     * This prevents PC broadcasts/remote nodes from reconfiguring a STM32. */
    if(r->h.src_device!=SA_DEVICE_LOCAL_ESP || r->h.dst_device!=SA_DEVICE_BROADCAST){
        DebugLog_Printf("[IDENTITY] rejected NODE_SET src=%lu dst=%lu (local ESP only)\r\n",
                        (unsigned long)r->h.src_device,(unsigned long)r->h.dst_device);
        return true;
    }
    if(r->h.payload_len!=12u){
        DebugLog_Printf("[IDENTITY] rejected NODE_SET len=%u\r\n",(unsigned)r->h.payload_len);
        return true;
    }

    uint8_t role=r->payload[0];
    uint32_t id;uint16_t net,grp;
    memcpy(&id,r->payload+4,4);memcpy(&net,r->payload+8,2);memcpy(&grp,r->payload+10,2);

    bool first_esp_sync=!identity_from_esp;
    const sa_node_config_t *before=ConfigManager_Get();
    uint8_t old_role=before->role;uint32_t old_id=before->device_id;
    uint16_t old_net=before->network_id,old_grp=before->group_id;
    sa_cfg_result_t res=ConfigManager_SetNode(role,id,net,grp);
    if(res!=SA_CFG_OK){
        DebugLog_Printf("[IDENTITY] ESP sync rejected role=%u device=%lu network=%u group=%u err=%d\r\n",
                        (unsigned)role,(unsigned long)id,(unsigned)net,(unsigned)grp,(int)res);
        return true;
    }

    identity_ready=true;
    identity_from_esp=true;
    last_esp_identity_ms=Board_Millis();

    const sa_node_config_t *after=ConfigManager_Get();
    bool identity_changed = old_role!=after->role || old_id!=after->device_id ||
                            old_net!=after->network_id || old_grp!=after->group_id || first_esp_sync;
    if(identity_changed){
        DebugLog_Printf("[IDENTITY] source=ESP role=%u->%u device=%lu->%lu network=%u->%u group=%u->%u gen=%lu\r\n",
                        (unsigned)old_role,(unsigned)after->role,
                        (unsigned long)old_id,(unsigned long)after->device_id,
                        (unsigned)old_net,(unsigned)after->network_id,
                        (unsigned)old_grp,(unsigned)after->group_id,
                        (unsigned long)ConfigManager_Generation());
    }
    identity_wait_reported=true;

    /* V2.4.6 bidirectional local-UART handshake.  Every valid NODE_SET must
     * produce a real STM32->ESP frame so UART TX health is observable without
     * waiting for a PC broadcast or a configured sensor.  SA_DEVICE_LOCAL_ESP
     * is consumed by the physically attached ESP and is never put on ESP-NOW. */
    (void)send_node_info(SA_DEVICE_LOCAL_ESP);

    /* Announce a newly synchronized identity to the PC for both roles.  MASTER
     * goes directly over USART2; SLAVE goes STM32->ESP->ESP-NOW->MASTER->PC. */
    if(identity_changed) (void)send_node_info(SA_DEVICE_PC);
    return true;
}

static void process_local(const sa_route_packet_t*r)
{
    uint32_t src=r->h.src_device;
    uint32_t detail=0;
    int res=0;
    switch(r->h.inner_cmd){
        case SA_CMD_LINK_HELLO:send_to(src,SA_CMD_LINK_ACK,0,0);break;
        case SA_CMD_LINK_ACK:case SA_CMD_CONTROL_RESULT:break;
        case SA_CMD_CAPS_GET:send_caps(src);break;
        case SA_CMD_CONFIG_GET:send_to(src,SA_CMD_CONFIG_REPLY,ConfigManager_Get(),sizeof(sa_node_config_t));break;
        case SA_CMD_CONFIG_BEGIN:ConfigManager_Begin();send_result(src,SA_CMD_CONFIG_BEGIN,0,0);break;
        case SA_CMD_CONFIG_SENSOR_SET:
            if(r->h.payload_len!=sizeof(sa_sensor_slot_config_t)){send_result(src,SA_CMD_CONFIG_SENSOR_SET,-1,0);break;}
            res=ConfigManager_SetSlot((const sa_sensor_slot_config_t*)r->payload);
            send_result(src,SA_CMD_CONFIG_SENSOR_SET,res,0);break;
        case SA_CMD_CONFIG_SENSOR_DEL:
            if(r->h.payload_len<1){send_result(src,SA_CMD_CONFIG_SENSOR_DEL,-1,0);break;}
            res=ConfigManager_DeleteSlot(r->payload[0]);
            send_result(src,SA_CMD_CONFIG_SENSOR_DEL,res,0);break;
        case SA_CMD_CONFIG_VALIDATE:
            res=SensorManager_ValidateConfig(ConfigManager_GetWorking(),&detail)?0:-10;
            send_result(src,SA_CMD_CONFIG_VALIDATE,res,detail);break;
        case SA_CMD_CONFIG_COMMIT:
            if(!SensorManager_ValidateConfig(ConfigManager_GetWorking(),&detail)){
                send_result(src,SA_CMD_CONFIG_COMMIT,-10,detail);break;
            }
            res=ConfigManager_Commit();
            if(!res)SensorManager_Reconfigure(ConfigManager_Get());
            send_result(src,SA_CMD_CONFIG_COMMIT,res,detail);break;
        case SA_CMD_CONFIG_ROLLBACK:
            ConfigManager_Rollback();send_result(src,SA_CMD_CONFIG_ROLLBACK,0,0);break;
        case SA_CMD_CONFIG_FACTORY:
            res=ConfigManager_FactoryReset();SensorManager_Reconfigure(ConfigManager_Get());
            send_result(src,SA_CMD_CONFIG_FACTORY,res,0);break;
        case SA_CMD_NODE_SET:
            /* Reaching this path means the command did not come from the local
             * ESP control ingress. Never permit network-side identity changes. */
            DebugLog_Printf("[IDENTITY] ignored network-side NODE_SET src=%lu\r\n",(unsigned long)src);
            if(src && src!=SA_DEVICE_LOCAL_ESP) send_result(src,SA_CMD_NODE_SET,SA_CFG_ERR_ARG,0);
            break;
        case SA_CMD_DIAG_TELEMETRY:
            if(r->h.payload_len!=sizeof(sa_diag_telemetry_t)){send_result(src,SA_CMD_DIAG_TELEMETRY,-1,0);break;}
            {
                const sa_diag_telemetry_t *d=(const sa_diag_telemetry_t*)r->payload;
                sa_telemetry_payload_t t;
                memset(&t,0,sizeof(t));
                t.slot_id=d->slot_id;t.count=1;t.driver_id=d->driver_id;t.timestamp_ms=Board_Millis();
                t.values[0].measurement_id=d->measurement_id;t.values[0].value_q16=d->value_q16;t.values[0].quality=d->quality;
                on_telemetry(&t);send_result(src,SA_CMD_DIAG_TELEMETRY,0,0);
            }
            break;
        case SA_CMD_OTA_INFO_GET:
            if(r->h.payload_len==sizeof(sa_ota_info_get_t) && ((const sa_ota_info_get_t*)r->payload)->component==SA_OTA_COMPONENT_STM32) send_ota_info(src);
            break;
        case SA_CMD_OTA_PREPARE:
            if(r->h.payload_len==sizeof(sa_ota_prepare_t) && ((const sa_ota_prepare_t*)r->payload)->component==SA_OTA_COMPONENT_STM32){
                uint32_t transfer=((const sa_ota_prepare_t*)r->payload)->transfer_id;
                if(transfer==0u){send_ota_result(src,0,SA_OTA_STATE_ERROR,-1,0,0);break;}
                send_ota_result(src,transfer,SA_OTA_STATE_READY,0,0,0);
                HAL_Delay(80);
                {
                    const sa_node_config_t *cfg = ConfigManager_Get();
                    OtaBoot_Request(transfer, cfg->device_id, cfg->group_id);
                }
            }else send_ota_result(src,0,SA_OTA_STATE_ERROR,-1,0,0);
            break;
        case SA_CMD_OTA_STATUS:
            if(r->h.payload_len==sizeof(sa_ota_simple_t) && ((const sa_ota_simple_t*)r->payload)->component==SA_OTA_COMPONENT_STM32){
                const sa_ota_simple_t*q=(const sa_ota_simple_t*)r->payload;send_ota_result(src,q->transfer_id,SA_OTA_STATE_IDLE,0,0,0);
            }
            break;
        case SA_CMD_CONTROL_GPIO:
            if(r->h.payload_len!=sizeof(sa_gpio_control_t)){send_result(src,SA_CMD_CONTROL_GPIO,-1,0);break;}
            res=ActuatorManager_Command((const sa_gpio_control_t*)r->payload)?0:-20;
            send_result(src,SA_CMD_CONTROL_GPIO,res,0);break;
        case SA_CMD_HEARTBEAT:send_node_info(src);break;
        default:send_result(src,r->h.inner_cmd,-100,0);break;
    }
}

/* Route ingress from attached ESP (USART1). */
static void on_route_from_esp(const sa_route_packet_t*r)
{
    if(!r)return;

    /* Identity sync must be accepted before consulting the old/fallback role. */
    if(handle_attached_esp_identity(r)) return;
    if(!identity_ready){
        /* Recovery exception: a persisted MASTER PC may configure/query its
         * physically attached ESP before that ESP has a valid Node identity.
         * Only the two radio-management reply shapes are allowed through. */
        if(radio_reply_from_attached_esp(r) && persisted_master_local_pc_available(ConfigManager_Get()))
            PcLink_SendRoute(r);
        return;
    }

    const sa_node_config_t*c=ConfigManager_Get();
    if(c->role==SA_ROLE_MASTER){
        if(r->h.dst_device==SA_DEVICE_PC){
            PcLink_SendRoute(r);
            return;
        }
        if(!local_dst(r)){
            /* MASTER may relay a route between remote nodes/groups. */
            CommLink_SendRoute(r);
            return;
        }
        if(!local_group_match(r)) return;
    }else{
        if(!local_dst(r) || !local_group_match(r)) return;
    }
    process_local(r);
}

/* Route ingress from PC/Web over USART2. */
static void on_route_from_pc(const sa_route_packet_t*r)
{
    if(!r)return;
    const sa_node_config_t*c=ConfigManager_Get();
    if(!local_pc_available(c))return;

    /* V2.4.4: while the attached ESP has not synchronized identity, keep only
     * LOCAL MASTER management alive. Never forward a PC request to USART1/radio
     * from a persisted fallback identity. This lets the upper computer connect
     * and diagnose ESP/UART1 failures without weakening radio identity safety. */
    if(!identity_ready){
        if(r->h.dst_device==SA_DEVICE_BROADCAST){
            if(r->h.inner_cmd!=SA_CMD_NODE_SET) process_local(r);
            return;
        }
        /* Local MASTER recovery path: allow only ESP-owned radio management
         * over the physical USART1 even before NODE_SET has synchronized. This
         * lets Web/Qt recover an unconfigured attached ESP without AT. */
        if(local_dst(r) && radio_targets_attached_esp(r)){
            (void)CommLink_SendRoute(r);
            return;
        }
        if(local_dst(r) && !ota_targets_attached_esp(r)) process_local(r);
        return;
    }

    if(r->h.dst_device==SA_DEVICE_BROADCAST){
        /* Never execute NODE_SET from PC broadcasts locally.  Only execute other
         * broadcasts on this MASTER when the logical group matches, while still
         * forwarding the original route so other groups can be managed. */
        if(r->h.inner_cmd!=SA_CMD_NODE_SET && local_group_match(r)) process_local(r);
        CommLink_SendRoute(r);
        return;
    }
    if(!local_dst(r)){
        CommLink_SendRoute(r);
        return;
    }
    if(!local_group_match(r)) return;
    if(ota_targets_attached_esp(r) || radio_targets_attached_esp(r)){
        CommLink_SendRoute(r);
        return;
    }
    process_local(r);
}

void NodeService_Init(void)
{
    CommLink_SetRouteCallback(on_route_from_esp);
    PcLink_SetRouteCallback(on_route_from_pc);
    SensorManager_SetTelemetryCallback(on_telemetry);
    last_hb=0;
    identity_ready=false;
    identity_from_esp=false;
    identity_wait_reported=false;
    identity_wait_started=Board_Millis();
    last_esp_identity_ms=0;
    DebugLog_Printf("[IDENTITY] waiting for attached ESP NODE_SET; persisted identity cannot activate radio routing\r\n");
    {
        const sa_node_config_t *stored=ConfigManager_Get();
        if(persisted_master_local_pc_available(stored)){
            DebugLog_Printf("[PC] local MASTER management enabled from persisted identity; radio remains WAIT_ESP until ESP NODE_SET\r\n");
        }
    }
}

void NodeService_Process(void)
{
    uint32_t now=Board_Millis();

    /* The ESP reasserts NODE_SET every 2 s.  Do not keep radio routing enabled
     * forever from a stale identity if the local UART/ESP disappears later. */
    if(identity_ready && last_esp_identity_ms!=0u && now-last_esp_identity_ms>=ESP_IDENTITY_LOSS_MS){
        identity_ready=false;
        identity_from_esp=false;
        identity_wait_started=now;
        identity_wait_reported=true;
        DebugLog_Printf("[IDENTITY] attached ESP heartbeat lost; radio routing disabled until a fresh NODE_SET\r\n");
    }

    if(!identity_ready && !identity_wait_reported && now-identity_wait_started>=ESP_IDENTITY_WARN_MS){
        identity_wait_reported=true;
        const sa_node_config_t *stored=ConfigManager_Get();
        DebugLog_Printf("[IDENTITY] ESP sync timeout; radio stays WAIT_ESP. stored role=%u device=%lu network=%u group=%u gen=%lu remains available only for boot/OTA recovery and local MASTER PC management\r\n",
                        (unsigned)stored->role,(unsigned long)stored->device_id,(unsigned)stored->network_id,
                        (unsigned)stored->group_id,(unsigned long)ConfigManager_Generation());
    }

    const sa_node_config_t*c=ConfigManager_Get();
    /* MASTER heartbeat stays directly on USART2.  A synchronized SLAVE also
     * emits a 5 s presence heartbeat through its ESP so Node1001+ remains
     * discoverable even when the upper computer did not just broadcast a probe. */
    if(now-last_hb>=5000u && (identity_ready || local_pc_available(c))){
        last_hb=now;
        node_info_t i={c->role,{0},c->device_id,c->network_id,c->group_id,ConfigManager_Generation()};
        (void)send_to(SA_DEVICE_PC,SA_CMD_HEARTBEAT,&i,sizeof(i));
    }
}
