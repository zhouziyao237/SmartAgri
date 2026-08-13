#include <stdint.h>
#include <stddef.h>

#define REG32(a) (*(volatile uint32_t*)(a))
#define REG16(a) (*(volatile uint16_t*)(a))
#define RCC_BASE 0x40021000u
#define RCC_CR REG32(RCC_BASE+0x00u)
#define RCC_CFGR REG32(RCC_BASE+0x04u)
#define RCC_APB2ENR REG32(RCC_BASE+0x18u)
#define RCC_APB1ENR REG32(RCC_BASE+0x1Cu)
#define FLASH_BASE_R 0x40022000u
#define FLASH_ACR REG32(FLASH_BASE_R+0x00u)
#define FLASH_KEYR REG32(FLASH_BASE_R+0x04u)
#define FLASH_SR REG32(FLASH_BASE_R+0x0Cu)
#define FLASH_CR REG32(FLASH_BASE_R+0x10u)
#define FLASH_AR REG32(FLASH_BASE_R+0x14u)
#define GPIOA_BASE 0x40010800u
#define GPIOA_CRL REG32(GPIOA_BASE+0x00u)
#define GPIOA_CRH REG32(GPIOA_BASE+0x04u)
#define USART1_BASE 0x40013800u
#define USART2_BASE 0x40004400u
#define USART_SR(b) REG32((b)+0x00u)
#define USART_DR(b) REG32((b)+0x04u)
#define USART_BRR(b) REG32((b)+0x08u)
#define USART_CR1(b) REG32((b)+0x0Cu)

/*
 * Bootloader UART contract (must match the application wiring):
 *   USART1 PA9/PA10 = PC/Web/debug @ 115200
 *   USART2 PA2/PA3  = ESP business/OTA @ 921600
 *
 * clock64() configures PCLK2=64 MHz and PCLK1=32 MHz. On STM32F1 with
 * oversampling by 16, BRR is the rounded PCLK/baud ratio. Keeping these
 * values symbolic prevents the bootloader from drifting away from the app.
 */
#define BOOT_PCLK2_HZ 64000000u
#define BOOT_PCLK1_HZ 32000000u
#define BOOT_PC_BAUD   115200u
#define BOOT_ESP_BAUD  921600u
#define USART_BRR_VALUE(pclk, baud) (((pclk) + ((baud) / 2u)) / (baud))

#define PWR_CR REG32(0x40007000u)
#define BKP_DR(n) REG16(0x40006C04u + (((uint32_t)(n)-1u)*4u))
#define SCB_VTOR REG32(0xE000ED08u)
#define SCB_AIRCR REG32(0xE000ED0Cu)
#define SYST_CSR REG32(0xE000E010u)

#define APP_BASE 0x08002000u
#define APP_END  0x0800F800u
#define APP_MAX  (APP_END-APP_BASE)
#define SRAM_BASE 0x20000000u
#define SRAM_END  0x20005000u
#define OTA_MAGIC 0xA55Au
#define OTA_SESSION_VERSION 0x0001u
#define CFG_SCHEMA 0x0201u
#define CFG_SIZE 384u
#define CFG_VALID 0xA55AA55Au
#define CFG_A 0x0800F800u
#define CFG_B 0x0800FC00u
#define CFG_MAGIC 0x53414346u
#define SA_SOF 0xA5u
#define SA_CMD_ROUTE 0x00E1u
#define SA_DEVICE_PC 0xFFFFFFFEu
#define SA_OTA_COMPONENT_STM32 1u
#define SA_OTA_TARGET_STM32F103C8 0x0101u
#define SA_CMD_OTA_INFO_GET 0x3400u
#define SA_CMD_OTA_INFO_REPLY 0x3401u
#define SA_CMD_OTA_PREPARE 0x3402u
#define SA_CMD_OTA_BEGIN 0x3403u
#define SA_CMD_OTA_DATA 0x3404u
#define SA_CMD_OTA_END 0x3405u
#define SA_CMD_OTA_ABORT 0x3406u
#define SA_CMD_OTA_STATUS 0x3407u
#define SA_CMD_OTA_RESULT 0x3408u
#define OTA_IDLE 0u
#define OTA_READY 1u
#define OTA_RX 2u
#define OTA_VERIFY 3u
#define OTA_REBOOT 4u
#define OTA_ERROR 5u
#define MAX_FRAME 521u
#define ROUTE_LEN 20u

static uint32_t dev_id=0u;static uint16_t group_id=0u;static uint8_t state=OTA_READY;static uint32_t transfer_id,image_size,next_offset,crc_expect,crc_state;
static uint8_t tx_seq1,tx_seq2;
typedef struct{uint8_t b[MAX_FRAME];uint16_t used,expected;} parser_t;static parser_t p1,p2;

static uint16_t rd16(const uint8_t*p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}static uint32_t rd32(const uint8_t*p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}static void wr16(uint8_t*p,uint16_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);}static void wr32(uint8_t*p,uint32_t v){p[0]=(uint8_t)v;p[1]=(uint8_t)(v>>8);p[2]=(uint8_t)(v>>16);p[3]=(uint8_t)(v>>24);}
static uint8_t crc8(const uint8_t*d,uint16_t n){uint8_t c=0xFFu;for(uint16_t i=0;i<n;i++){c^=d[i];for(uint8_t b=0;b<8;b++)c=(c&0x80u)?(uint8_t)((c<<1)^0x07u):(uint8_t)(c<<1);}return c;}
static uint16_t crc16(const uint8_t*d,uint16_t n){uint16_t c=0xFFFFu;for(uint16_t i=0;i<n;i++){c^=(uint16_t)d[i]<<8;for(uint8_t b=0;b<8;b++)c=(c&0x8000u)?(uint16_t)((c<<1)^0x1021u):(uint16_t)(c<<1);}return c;}
static uint32_t crc32_up(uint32_t c,const uint8_t*d,uint16_t n){for(uint16_t i=0;i<n;i++){c^=d[i];for(uint8_t b=0;b<8;b++)c=(c&1u)?(c>>1)^0xEDB88320u:c>>1;}return c;}

static int app_valid(void){uint32_t sp=REG32(APP_BASE),rv=REG32(APP_BASE+4u);uint32_t addr=rv&~1u;return (sp>=SRAM_BASE&&sp<=SRAM_END&&(sp&3u)==0u&&(rv&1u)&&addr>=APP_BASE&&addr<APP_END);}

/*
 * V2.4.5 boot hand-off
 * ---------------------
 * Never change MSP and then continue in ordinary C.  A compiler is allowed to
 * spill/reload locals through the current stack, so the old implementation
 * could load a bogus Reset_Handler after MSP was replaced.  It also left
 * PRIMASK=1, which prevents HAL SysTick from advancing and can make clock
 * fallback/timeouts hang before the first UART log.
 *
 * Pass app SP/Reset_Handler in r0/r1 and perform the final hand-off in a naked
 * function.  After MSP changes there are no further C accesses or stack use.
 */
__attribute__((naked,noreturn)) static void jump_app_raw(uint32_t sp __attribute__((unused)),uint32_t rv __attribute__((unused)))
{
    __asm volatile(
        "cpsid i\n"

        /* Stop SysTick and clear its reload/current state. */
        "ldr r2, =0xE000E010\n"
        "movs r3, #0\n"
        "str r3, [r2, #0]\n"
        "str r3, [r2, #4]\n"
        "str r3, [r2, #8]\n"

        /* Disable all STM32F103 external IRQs and clear pending IRQs. */
        "ldr r2, =0xE000E180\n"
        "mvns r3, r3\n"
        "str r3, [r2, #0]\n"
        "str r3, [r2, #4]\n"
        "ldr r2, =0xE000E280\n"
        "str r3, [r2, #0]\n"
        "str r3, [r2, #4]\n"

        /* Clear pending SysTick/PendSV in SCB->ICSR. */
        "ldr r2, =0xE000ED04\n"
        "ldr r3, =0x0A000000\n"
        "str r3, [r2]\n"

        /* Relocate vectors before interrupts are restored. */
        "ldr r2, =0xE000ED08\n"
        "ldr r3, =0x08002000\n"
        "str r3, [r2]\n"
        "dsb\n"
        "isb\n"

        /* Restore reset-like execution state. r0=MSP, r1=Reset_Handler. */
        "movs r2, #0\n"
        "msr control, r2\n"
        "msr basepri, r2\n"
        "msr faultmask, r2\n"
        "msr msp, r0\n"
        "msr primask, r2\n"
        "dsb\n"
        "isb\n"
        "bx r1\n"
    );
}
static void jump_app(void){uint32_t sp=REG32(APP_BASE),rv=REG32(APP_BASE+4u);jump_app_raw(sp,rv);}
static void sys_reset(void){SCB_AIRCR=0x05FA0004u;for(;;);}

static uint32_t crc32_mem(uint32_t addr,uint32_t n)
{
    uint32_t c=0xFFFFFFFFu;
    for(uint32_t i=0;i<n;i++){
        c^=*(const volatile uint8_t*)(uintptr_t)(addr+i);
        for(uint8_t b=0;b<8;b++)c=(c&1u)?(c>>1)^0xEDB88320u:c>>1;
    }
    return ~c;
}

static int cfg_page_valid(uint32_t page)
{
    if(REG32(page+0u)!=CFG_MAGIC)return 0;
    if(REG16(page+4u)!=CFG_SCHEMA || REG16(page+6u)!=CFG_SIZE)return 0;
    if(REG32(page+400u)!=CFG_VALID)return 0;
    return REG32(page+12u)==crc32_mem(page+16u,CFG_SIZE);
}

static void identity(void)
{
    const int va=cfg_page_valid(CFG_A),vb=cfg_page_valid(CFG_B);
    uint32_t page=0;
    if(va&&vb)page=(REG32(CFG_A+8u)>=REG32(CFG_B+8u))?CFG_A:CFG_B;
    else if(va)page=CFG_A;
    else if(vb)page=CFG_B;
    if(page){
        uint32_t d=REG32(page+20u);
        uint16_t g=REG16(page+26u);
        if(d)dev_id=d;
        group_id=g;
    }
}

static uint16_t session_crc(uint16_t version,uint16_t dev_lo,uint16_t dev_hi,
                            uint16_t group,uint16_t tid_lo,uint16_t tid_hi)
{
    uint16_t c=0x6D39u;
    const uint16_t w[6]={version,dev_lo,dev_hi,group,tid_lo,tid_hi};
    for(uint8_t i=0;i<6;i++){
        c^=w[i];
        c=(uint16_t)((c<<5)|(c>>11));
        c=(uint16_t)(c+0x9E37u);
    }
    return c;
}

static void backup_enable(void)
{
    RCC_APB1ENR|=(1u<<27)|(1u<<28); /* BKP + PWR */
    PWR_CR|=(1u<<8);                /* DBP */
}

static void session_store(uint32_t device,uint16_t group,uint32_t tid)
{
    uint16_t dlo=(uint16_t)device,dhi=(uint16_t)(device>>16);
    uint16_t tlo=(uint16_t)tid,thi=(uint16_t)(tid>>16);
    backup_enable();
    BKP_DR(1)=0u;
    BKP_DR(2)=OTA_SESSION_VERSION;
    BKP_DR(3)=dlo;
    BKP_DR(4)=dhi;
    BKP_DR(5)=group;
    BKP_DR(6)=tlo;
    BKP_DR(7)=thi;
    BKP_DR(8)=session_crc(OTA_SESSION_VERSION,dlo,dhi,group,tlo,thi);
    BKP_DR(1)=OTA_MAGIC;
}

static void session_clear(void)
{
    backup_enable();
    BKP_DR(1)=0u;
    BKP_DR(8)=0u;
}

static int session_load(void)
{
    backup_enable();
    if(BKP_DR(1)!=OTA_MAGIC || BKP_DR(2)!=OTA_SESSION_VERSION)return 0;
    uint16_t dlo=BKP_DR(3),dhi=BKP_DR(4),g=BKP_DR(5),tlo=BKP_DR(6),thi=BKP_DR(7);
    if(BKP_DR(8)!=session_crc(BKP_DR(2),dlo,dhi,g,tlo,thi))return 0;
    uint32_t d=(uint32_t)dlo|((uint32_t)dhi<<16);
    uint32_t t=(uint32_t)tlo|((uint32_t)thi<<16);
    if(!d || !t)return 0;
    dev_id=d;
    group_id=g;
    transfer_id=t;
    return 1;
}

static void clock64(void)
{
    FLASH_ACR=0x12u;
    RCC_CR|=1u;
    while(!(RCC_CR&2u)) { }
    RCC_CFGR=0u;
    RCC_CFGR|=(4u<<8);      /* APB1 = HCLK / 2 */
    RCC_CFGR|=(14u<<18);    /* PLLMUL x16 from HSI/2 = 64 MHz */
    RCC_CR|=(1u<<24);
    while(!(RCC_CR&(1u<<25))) { }
    RCC_CFGR=(RCC_CFGR&~3u)|2u;
    while(((RCC_CFGR>>2)&3u)!=2u) { }
}
static void uart_init(void)
{
    RCC_APB2ENR|=(1u<<2)|(1u<<14);
    RCC_APB1ENR|=(1u<<17);

    /* PA9=USART1_TX, PA10=USART1_RX; PA2=USART2_TX, PA3=USART2_RX. */
    GPIOA_CRH=(GPIOA_CRH&~(0xFFu<<4))|(0x4Bu<<4);
    GPIOA_CRL=(GPIOA_CRL&~(0xFFu<<8))|(0x4Bu<<8);

    /* PC/Web/debug side: USART1 @ 115200, PCLK2=64 MHz. */
    USART_BRR(USART1_BASE)=USART_BRR_VALUE(BOOT_PCLK2_HZ,BOOT_PC_BAUD);
    USART_CR1(USART1_BASE)=(1u<<13)|(1u<<3)|(1u<<2);

    /* ESP business/OTA side: USART2 @ 921600, PCLK1=32 MHz. */
    USART_BRR(USART2_BASE)=USART_BRR_VALUE(BOOT_PCLK1_HZ,BOOT_ESP_BAUD);
    USART_CR1(USART2_BASE)=(1u<<13)|(1u<<3)|(1u<<2);
}
static int rx_ready(uint32_t u){return (USART_SR(u)&0x20u)!=0u;}
static uint8_t rx_byte(uint32_t u){return (uint8_t)USART_DR(u);}
static void tx_byte(uint32_t u,uint8_t b){while(!(USART_SR(u)&0x80u)) { } USART_DR(u)=b;}
static void tx_buf(uint32_t u,const uint8_t*d,uint16_t n){for(uint16_t i=0;i<n;i++)tx_byte(u,d[i]);while(!(USART_SR(u)&0x40u)) { }}

static int flash_wait(void){uint32_t guard=8000000u;while((FLASH_SR&1u)&&--guard) { } return guard?0:-1;}
static void flash_unlock(void){if(FLASH_CR&(1u<<7)){FLASH_KEYR=0x45670123u;FLASH_KEYR=0xCDEF89ABu;}}
static int erase_app(uint32_t n){flash_unlock();uint32_t pages=(n+1023u)/1024u;for(uint32_t i=0;i<pages;i++){if(flash_wait())return -1;FLASH_CR|=(1u<<1);FLASH_AR=APP_BASE+i*1024u;FLASH_CR|=(1u<<6);if(flash_wait())return -2;FLASH_CR&=~(1u<<1);if(FLASH_SR&(1u<<5))FLASH_SR=(1u<<5);}return 0;}
static int program(uint32_t off,const uint8_t*d,uint16_t n){flash_unlock();for(uint16_t i=0;i<n;i+=2u){uint16_t h=d[i];if(i+1u<n)h|=(uint16_t)d[i+1u]<<8;else h|=0xFF00u;if(flash_wait())return -1;FLASH_CR|=1u;REG16(APP_BASE+off+i)=h;if(flash_wait())return -2;FLASH_CR&=~1u;if(REG16(APP_BASE+off+i)!=h)return -3;}return 0;}

static void send_route(uint32_t uart,uint32_t dst,uint16_t cmd,const uint8_t*pl,uint16_t plen){uint8_t out[80];uint16_t route_n=(uint16_t)(ROUTE_LEN+plen),frame_n=(uint16_t)(9u+route_n);uint8_t seq=(uart==USART1_BASE)?tx_seq1++:tx_seq2++;out[0]=SA_SOF;wr16(out+1,route_n);out[3]=seq;out[4]=crc8(out,4);wr16(out+5,SA_CMD_ROUTE);uint8_t*r=out+7;r[0]=2;r[1]=0;r[2]=1;r[3]=0;wr32(r+4,dev_id);wr32(r+8,dst);wr16(r+12,group_id);wr16(r+14,cmd);wr16(r+16,plen);wr16(r+18,0);for(uint16_t i=0;i<plen;i++)r[20+i]=pl[i];wr16(out+frame_n-2,crc16(out,frame_n-2));tx_buf(uart,out,frame_n);}
static void result(uint32_t u,uint32_t dst,uint32_t tid,int16_t code,uint32_t detail){uint8_t p[16];p[0]=SA_OTA_COMPONENT_STM32;p[1]=state;wr16(p+2,(uint16_t)code);wr32(p+4,tid);wr32(p+8,next_offset);wr32(p+12,detail);send_route(u,dst,SA_CMD_OTA_RESULT,p,16);}
static void info(uint32_t u,uint32_t dst){uint8_t p[16];p[0]=SA_OTA_COMPONENT_STM32;p[1]=state;wr16(p+2,SA_OTA_TARGET_STM32F103C8);wr32(p+4,0x00020407u);wr32(p+8,APP_MAX);wr32(p+12,3u);send_route(u,dst,SA_CMD_OTA_INFO_REPLY,p,16);}

static void process_route(uint32_t u,const uint8_t*r,uint16_t n)
{
    if(n<ROUTE_LEN||r[0]!=2)return;
    uint32_t src=rd32(r+4),dst=rd32(r+8);
    uint16_t cmd=rd16(r+14),plen=rd16(r+16);
    if(dst!=dev_id)return; /* destructive OTA commands are never accepted by broadcast */
    if(n!=(uint16_t)(ROUTE_LEN+plen))return;
    const uint8_t*p=r+ROUTE_LEN;

    if(cmd==SA_CMD_OTA_INFO_GET&&plen==4&&p[0]==SA_OTA_COMPONENT_STM32){info(u,src);return;}
    if(plen<1||p[0]!=SA_OTA_COMPONENT_STM32)return;

    if(cmd==SA_CMD_OTA_PREPARE&&plen==8){
        uint32_t tid=rd32(p+4);
        if(!tid){state=OTA_ERROR;result(u,src,tid,-1,0);return;}
        transfer_id=tid;
        next_offset=0;
        state=OTA_READY;
        session_store(dev_id,group_id,tid);
        result(u,src,tid,0,0);
        return;
    }

    if(cmd==SA_CMD_OTA_BEGIN&&plen==16){
        uint16_t target=rd16(p+2);
        uint32_t tid=rd32(p+4),sz=rd32(p+8),crc=rd32(p+12);
        if(!tid){state=OTA_ERROR;result(u,src,0,-1,0);return;}
        if(transfer_id&&tid!=transfer_id){result(u,src,tid,-5,transfer_id);return;}
        if(target!=SA_OTA_TARGET_STM32F103C8||sz==0||sz>APP_MAX){state=OTA_ERROR;next_offset=0;result(u,src,tid,-2,APP_MAX);return;}
        transfer_id=tid;
        session_store(dev_id,group_id,tid);
        state=OTA_READY;
        next_offset=0;
        if(erase_app(sz)){state=OTA_ERROR;result(u,src,tid,-3,0);return;}
        image_size=sz;
        crc_expect=crc;
        crc_state=0xFFFFFFFFu;
        state=OTA_RX;
        result(u,src,tid,0,0);
        return;
    }

    if(cmd==SA_CMD_OTA_DATA&&plen>=12){
        uint16_t dl=rd16(p+2);
        uint32_t tid=rd32(p+4),off=rd32(p+8);
        if(tid!=transfer_id||state!=OTA_RX){result(u,src,tid,-11,transfer_id);return;}
        if(dl!=(uint16_t)(plen-12)||!dl){result(u,src,tid,-12,plen-12);return;}
        if(off<next_offset&&off+dl==next_offset){result(u,src,tid,0,0);return;}
        if(off!=next_offset||next_offset+dl>image_size){result(u,src,tid,-12,next_offset);return;}
        if(program(off,p+12,dl)){state=OTA_ERROR;result(u,src,tid,-13,off);return;}
        crc_state=crc32_up(crc_state,p+12,dl);
        next_offset+=dl;
        result(u,src,tid,0,0);
        return;
    }

    if(cmd==SA_CMD_OTA_END&&plen==12){
        uint32_t tid=rd32(p+4),crc=rd32(p+8);
        if(tid!=transfer_id||state!=OTA_RX||next_offset!=image_size){result(u,src,tid,-21,next_offset);return;}
        state=OTA_VERIFY;
        uint32_t got=~crc_state;
        if(got!=crc_expect||crc!=crc_expect||!app_valid()){state=OTA_ERROR;result(u,src,tid,-22,got);return;}
        state=OTA_REBOOT;
        result(u,src,tid,0,got);
        for(volatile uint32_t d=0;d<200000u;d++) { }
        session_clear();
        sys_reset();
    }

    if(cmd==SA_CMD_OTA_ABORT&&plen==8){
        uint32_t tid=rd32(p+4);
        state=OTA_READY;
        transfer_id=tid;
        next_offset=0;
        result(u,src,tid,0,0);
        session_clear();
        transfer_id=0;
        return;
    }

    if(cmd==SA_CMD_OTA_STATUS&&plen==8){
        uint32_t tid=rd32(p+4);
        if(transfer_id&&tid&&tid!=transfer_id){result(u,src,tid,-5,transfer_id);return;}
        result(u,src,tid?tid:transfer_id,0,0);
        return;
    }
}
static void feed(uint32_t u,parser_t*q,uint8_t b){if(q->used==0&&b!=SA_SOF)return;if(q->used>=MAX_FRAME){q->used=q->expected=0;return;}q->b[q->used++]=b;if(q->used==5){uint16_t pl=rd16(q->b+1);if(pl>512u||crc8(q->b,4)!=q->b[4]){q->used=q->expected=0;return;}q->expected=(uint16_t)(9u+pl);}if(q->expected&&q->used==q->expected){uint16_t n=q->expected;if(crc16(q->b,n-2)==rd16(q->b+n-2)&&rd16(q->b+5)==SA_CMD_ROUTE)process_route(u,q->b+7,rd16(q->b+1));q->used=q->expected=0;}}

void boot_main(void){
    identity();
    int req=session_load(); /* OTA session overrides normal config identity. */
    if(!req&&app_valid())jump_app();
    clock64();
    uart_init();
    state=OTA_READY;
    for(;;){
        if(rx_ready(USART1_BASE))feed(USART1_BASE,&p1,rx_byte(USART1_BASE));
        if(rx_ready(USART2_BASE))feed(USART2_BASE,&p2,rx_byte(USART2_BASE));
    }
}
