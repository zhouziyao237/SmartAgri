#include "config_manager.h"
#include "board.h"
#include <string.h>
#include <stddef.h>

#define CFG_MAGIC 0x53414346u /* SACF */
#define CFG_VALID 0xA55AA55Au
#define PAGE_A 0x0800F800u
#define PAGE_B 0x0800FC00u
#define RESERVED_DEVICE_PC        0xFFFFFFFEu
#define RESERVED_DEVICE_BROADCAST 0xFFFFFFFFu

#pragma pack(push,1)
typedef struct {
    uint32_t magic;
    uint16_t schema;
    uint16_t size;
    uint32_t generation;
    uint32_t crc32;
    sa_node_config_t cfg;
    uint32_t valid;
} cfg_blob_t;
#pragma pack(pop)

_Static_assert(sizeof(sa_node_config_t)==384u,"Bootloader CFG_SIZE must be updated if sa_node_config_t changes");
_Static_assert(offsetof(sa_node_config_t,device_id)==4u,"Bootloader device_id offset mismatch");
_Static_assert(offsetof(sa_node_config_t,group_id)==10u,"Bootloader group_id offset mismatch");

static sa_node_config_t active_cfg;
static sa_node_config_t working_cfg;
static uint32_t generation;
static uint32_t active_page;
static bool transaction_active;

static uint32_t crc32_soft(const uint8_t*d,size_t n)
{
    uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++){
        c^=d[i];
        for(int b=0;b<8;b++) c=(c&1u)?(c>>1)^0xEDB88320u:c>>1;
    }
    return ~c;
}

static bool load_page(uint32_t a,cfg_blob_t*out)
{
    memcpy(out,(const void*)(uintptr_t)a,sizeof(*out));
    if(out->magic!=CFG_MAGIC || out->valid!=CFG_VALID ||
       out->schema!=SA_CONFIG_SCHEMA_VERSION || out->size!=sizeof(sa_node_config_t)) return false;
    return out->crc32==crc32_soft((const uint8_t*)&out->cfg,sizeof(out->cfg));
}

/* STM32 does not invent a role/device identity. The attached ESP supplies it.
 * A persisted identity is only a reboot/OTA fallback until ESP re-asserts it. */
static void defaults(sa_node_config_t*c)
{
    memset(c,0,sizeof(*c));
    c->schema_version=SA_CONFIG_SCHEMA_VERSION;
    c->role=SA_ROLE_UNCONFIGURED;
    c->device_id=0u;
    c->network_id=0u;
    c->group_id=0u;
    for(uint8_t i=0;i<SA_MAX_SENSOR_SLOTS;i++) c->slots[i].slot_id=i;
}

static bool identity_values_valid(uint8_t role,uint32_t id,uint16_t net)
{
    if(role!=SA_ROLE_MASTER && role!=SA_ROLE_SLAVE) return false;
    if(id==0u || id==RESERVED_DEVICE_PC || id==RESERVED_DEVICE_BROADCAST) return false;
    if(net==0u) return false;
    return true;
}

static void copy_identity(sa_node_config_t *dst,const sa_node_config_t *src)
{
    dst->role=src->role;
    dst->device_id=src->device_id;
    dst->network_id=src->network_id;
    dst->group_id=src->group_id;
}

void ConfigManager_Init(void)
{
    cfg_blob_t a,b;
    bool va=load_page(PAGE_A,&a),vb=load_page(PAGE_B,&b);
    if(va&&(!vb||a.generation>=b.generation)){
        active_cfg=a.cfg;generation=a.generation;active_page=PAGE_A;
    }else if(vb){
        active_cfg=b.cfg;generation=b.generation;active_page=PAGE_B;
    }else{
        defaults(&active_cfg);generation=0;active_page=0;
    }
    active_cfg.schema_version=SA_CONFIG_SCHEMA_VERSION;
    working_cfg=active_cfg;
    transaction_active=false;
}

const sa_node_config_t*ConfigManager_Get(void){return &active_cfg;}
const sa_node_config_t*ConfigManager_GetWorking(void){return &working_cfg;}
uint32_t ConfigManager_Generation(void){return generation;}
bool ConfigManager_HasPersistedConfig(void){return active_page!=0u;}
bool ConfigManager_IdentityValid(void){return identity_values_valid(active_cfg.role,active_cfg.device_id,active_cfg.network_id);}
bool ConfigManager_TransactionActive(void){return transaction_active;}

void ConfigManager_Begin(void)
{
    working_cfg=active_cfg;
    transaction_active=true;
}

sa_cfg_result_t ConfigManager_SetSlot(const sa_sensor_slot_config_t*s)
{
    if(!s||s->slot_id>=SA_MAX_SENSOR_SLOTS)return SA_CFG_ERR_SLOT;
    if(!transaction_active) ConfigManager_Begin();
    working_cfg.slots[s->slot_id]=*s;
    uint8_t n=0;
    for(unsigned i=0;i<SA_MAX_SENSOR_SLOTS;i++)if(working_cfg.slots[i].enabled)n++;
    working_cfg.sensor_count=n;
    return SA_CFG_OK;
}

sa_cfg_result_t ConfigManager_DeleteSlot(uint8_t id)
{
    if(id>=SA_MAX_SENSOR_SLOTS)return SA_CFG_ERR_SLOT;
    if(!transaction_active) ConfigManager_Begin();
    memset(&working_cfg.slots[id],0,sizeof(working_cfg.slots[id]));
    working_cfg.slots[id].slot_id=id;
    uint8_t n=0;
    for(unsigned i=0;i<SA_MAX_SENSOR_SLOTS;i++)if(working_cfg.slots[i].enabled)n++;
    working_cfg.sensor_count=n;
    return SA_CFG_OK;
}

void ConfigManager_Rollback(void)
{
    working_cfg=active_cfg;
    transaction_active=false;
}

static sa_cfg_result_t write_page(uint32_t page,const cfg_blob_t*b)
{
    FLASH_EraseInitTypeDef e={0};uint32_t err=0;
    e.TypeErase=FLASH_TYPEERASE_PAGES;e.PageAddress=page;e.NbPages=1;
    HAL_FLASH_Unlock();
    if(HAL_FLASHEx_Erase(&e,&err)!=HAL_OK){HAL_FLASH_Lock();return SA_CFG_ERR_FLASH;}
    size_t halfwords=(sizeof(*b)+1u)/2u;
    for(size_t i=0;i<halfwords;i++){
        uint16_t half=0xFFFFu;size_t off=i*2u;size_t rem=sizeof(*b)-off;
        memcpy(&half,((const uint8_t*)b)+off,rem>=2u?2u:rem);
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,page+(uint32_t)i*2u,half)!=HAL_OK){
            HAL_FLASH_Lock();return SA_CFG_ERR_FLASH;
        }
    }
    HAL_FLASH_Lock();
    cfg_blob_t check;
    if(!load_page(page,&check))return SA_CFG_ERR_CRC;
    return SA_CFG_OK;
}

static sa_cfg_result_t persist_candidate(const sa_node_config_t *candidate)
{
    cfg_blob_t b;
    memset(&b,0xFF,sizeof(b));
    b.magic=CFG_MAGIC;
    b.schema=SA_CONFIG_SCHEMA_VERSION;
    b.size=sizeof(sa_node_config_t);
    b.generation=generation+1u;
    b.cfg=*candidate;
    b.cfg.schema_version=SA_CONFIG_SCHEMA_VERSION;
    b.crc32=crc32_soft((const uint8_t*)&b.cfg,sizeof(b.cfg));
    b.valid=CFG_VALID;
    uint32_t target=(active_page==PAGE_A)?PAGE_B:PAGE_A;
    sa_cfg_result_t r=write_page(target,&b);
    if(r==SA_CFG_OK){
        active_cfg=b.cfg;
        generation=b.generation;
        active_page=target;
    }
    return r;
}

sa_cfg_result_t ConfigManager_Commit(void)
{
    /* A sensor transaction may have been open while the attached ESP changed
     * identity. Never let stale transaction data roll identity backwards. */
    sa_node_config_t candidate=working_cfg;
    copy_identity(&candidate,&active_cfg);
    candidate.schema_version=SA_CONFIG_SCHEMA_VERSION;
    sa_cfg_result_t r=persist_candidate(&candidate);
    if(r==SA_CFG_OK){
        working_cfg=active_cfg;
        transaction_active=false;
    }
    return r;
}

sa_cfg_result_t ConfigManager_FactoryReset(void)
{
    /* "Factory reset" from the sensor UI must not turn a MASTER into the
     * compiled-in SLAVE/default identity. Identity belongs to the ESP. */
    sa_node_config_t candidate;
    defaults(&candidate);
    copy_identity(&candidate,&active_cfg);
    sa_cfg_result_t r=persist_candidate(&candidate);
    if(r==SA_CFG_OK){
        working_cfg=active_cfg;
        transaction_active=false;
    }
    return r;
}

sa_cfg_result_t ConfigManager_SetNode(uint8_t role,uint32_t id,uint16_t net,uint16_t group)
{
    if(!identity_values_valid(role,id,net))return SA_CFG_ERR_ARG;

    bool same=active_cfg.role==role && active_cfg.device_id==id &&
              active_cfg.network_id==net && active_cfg.group_id==group;

    /* Keep an open sensor transaction aligned with the latest ESP identity even
     * when no flash write is needed. */
    if(transaction_active){
        working_cfg.role=role;
        working_cfg.device_id=id;
        working_cfg.network_id=net;
        working_cfg.group_id=group;
    }

    if(same && active_page!=0u) return SA_CFG_OK;

    sa_node_config_t candidate=active_cfg;
    candidate.role=role;
    candidate.device_id=id;
    candidate.network_id=net;
    candidate.group_id=group;
    candidate.schema_version=SA_CONFIG_SCHEMA_VERSION;

    sa_cfg_result_t r=persist_candidate(&candidate);
    if(r==SA_CFG_OK && !transaction_active) working_cfg=active_cfg;
    return r;
}
