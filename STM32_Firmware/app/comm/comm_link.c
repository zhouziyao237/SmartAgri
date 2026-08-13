#include "comm_link.h"
#include "board.h"
#include "debug_log.h"
#include "debug_shell.h"
#include <string.h>

#define COMM_UART_HANDLE huart2
#define COMM_DMA_HANDLE  hdma_usart2_rx
#define COMM_USART       USART2
#define COMM_DMA_CH      DMA1_Channel6
#define COMM_DMA_IRQn    DMA1_Channel6_IRQn
#define COMM_UART_NAME   "USART2"
#define COMM_DMA_NAME    "DMA1_Channel6"

/*
 * V2.4.3 UART RX design
 * -----------------------
 * V2.4.2 used HAL_UARTEx_ReceiveToIdle_DMA() in DMA_NORMAL mode and re-armed
 * reception from HAL_UARTEx_RxEventCallback().  If starting/re-arming failed
 * because of a pending UART error/overrun, the return value was ignored and
 * USART1 could remain permanently deaf while the ESP continued transmitting.
 *
 * V2.4.3 uses one continuously-running circular DMA receiver.  CommLink_Process
 * polls DMA CNDTR and drains newly written bytes into the parser ring.  The RX
 * path therefore does not depend on IDLE callbacks or on repeated DMA re-arm.
 */
#define DMA_RX_SZ 256u
#define RAW_RING_SZ 1024u

static uint8_t dma_rx[DMA_RX_SZ];
static uint16_t dma_last_pos;
static volatile uint16_t ring_head,ring_tail;
static uint8_t ring[RAW_RING_SZ];
static sa_frame_parser_t parser;
static comm_route_cb_t route_cb;
static uint8_t tx_buf[SA_MAX_FRAME];
static uint8_t tx_seq;
static volatile uint32_t tx_attempts;
static volatile uint32_t tx_ok_frames;
static volatile uint32_t tx_bytes;
static volatile uint32_t tx_hal_fail;
static volatile uint32_t tx_encode_fail;
static volatile uint32_t tx_ready_high;
static volatile uint32_t tx_ready_low;
static uint32_t rx_drop;
static volatile uint32_t raw_rx_bytes;
static volatile uint32_t dma_rx_restarts;
static volatile uint32_t dma_rx_start_fail;
static volatile uint32_t uart_error_events;
static volatile uint32_t uart_error_ore;
static volatile uint32_t uart_error_fe;
static volatile uint32_t uart_error_ne;
static volatile uint32_t uart_error_pe;
static volatile bool rx_restart_pending;

static bool dma_contract_ok(void)
{
    DMA_Channel_TypeDef *ch = COMM_DMA_CH;

    if(COMM_UART_HANDLE.Instance != COMM_USART) return false;
    if(COMM_UART_HANDLE.hdmarx != &COMM_DMA_HANDLE) return false;
    if(COMM_DMA_HANDLE.Instance != ch) return false;
    if((COMM_UART_HANDLE.Instance->CR1 & (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE)) !=
       (USART_CR1_UE | USART_CR1_RE | USART_CR1_TE)) return false;
    if((COMM_UART_HANDLE.Instance->CR3 & USART_CR3_DMAR) == 0u) return false;
    if((ch->CCR & DMA_CCR_EN) == 0u) return false;
    if((ch->CCR & DMA_CCR_MINC) == 0u) return false;
    if((ch->CCR & DMA_CCR_DIR) != 0u) return false;
    if((ch->CCR & DMA_CCR_CIRC) == 0u) return false;
    if(ch->CPAR != (uint32_t)&COMM_USART->DR) return false;
    if(ch->CMAR != (uint32_t)dma_rx) return false;
    if(ch->CNDTR == 0u || ch->CNDTR > DMA_RX_SZ) return false;
    return true;
}

static void clear_uart_rx_error_flags(void)
{
    /* On STM32F1, reading SR then DR clears PE/FE/NE/ORE and RXNE. */
    volatile uint32_t sr = COMM_UART_HANDLE.Instance->SR;
    volatile uint32_t dr = COMM_UART_HANDLE.Instance->DR;
    (void)sr;
    (void)dr;
}

static bool start_circular_rx(void)
{
    /* Make restart deterministic even if the previous HAL state was aborted by
     * an overrun/error interrupt. */
    (void)HAL_UART_AbortReceive(&COMM_UART_HANDLE);
    clear_uart_rx_error_flags();
    dma_last_pos = 0u;

    HAL_StatusTypeDef st = HAL_UART_Receive_DMA(&COMM_UART_HANDLE,dma_rx,DMA_RX_SZ);
    if(st != HAL_OK || !dma_contract_ok()){
        dma_rx_start_fail++;
        (void)HAL_UART_AbortReceive(&COMM_UART_HANDLE);
        CLEAR_BIT(COMM_UART_HANDLE.Instance->CR3, USART_CR3_DMAR);
        if(COMM_UART_HANDLE.hdmarx) (void)HAL_DMA_Abort(COMM_UART_HANDLE.hdmarx);
        COMM_UART_HANDLE.RxState = HAL_UART_STATE_READY;
        return false;
    }

    /* RX is consumed by polling DMA CNDTR in CommLink_Process().  No HT/TC ISR
     * is needed; disabling these also avoids callbacks racing the main-loop
     * consumer. UART error IRQs remain enabled by HAL_UART_Receive_DMA(). */
    if(COMM_UART_HANDLE.hdmarx){
        __HAL_DMA_DISABLE_IT(COMM_UART_HANDLE.hdmarx,DMA_IT_HT);
        __HAL_DMA_DISABLE_IT(COMM_UART_HANDLE.hdmarx,DMA_IT_TC);
    }
    dma_rx_restarts++;
    return true;
}

void CommLink_Init(void)
{
    ring_head=ring_tail=0;
    tx_seq=0u;
    tx_attempts=0u;
    tx_ok_frames=0u;
    tx_bytes=0u;
    tx_hal_fail=0u;
    tx_encode_fail=0u;
    tx_ready_high=0u;
    tx_ready_low=0u;
    rx_drop=0u;
    raw_rx_bytes=0u;
    dma_rx_restarts=0u;
    dma_rx_start_fail=0u;
    uart_error_events=0u;
    uart_error_ore=0u;
    uart_error_fe=0u;
    uart_error_ne=0u;
    uart_error_pe=0u;
    rx_restart_pending=false;
    sa_frame_parser_init(&parser);
    (void)start_circular_rx();
}

void CommLink_SetRouteCallback(comm_route_cb_t cb){route_cb=cb;}

static void ring_push(const uint8_t*d,uint16_t n)
{
    for(uint16_t i=0;i<n;i++){
        uint16_t next=(uint16_t)((ring_head+1u)%RAW_RING_SZ);
        if(next==ring_tail){rx_drop++;break;}
        ring[ring_head]=d[i];
        ring_head=next;
    }
}

static void poll_dma_rx(void)
{
    if(rx_restart_pending){
        rx_restart_pending=false;
        (void)start_circular_rx();
    }
    if(!COMM_UART_HANDLE.hdmarx || COMM_UART_HANDLE.RxState==HAL_UART_STATE_READY || !dma_contract_ok()){
        /* A prior error may have ended RX before the callback reached us. */
        if(!rx_restart_pending) (void)start_circular_rx();
        return;
    }

    uint16_t pos=(uint16_t)(DMA_RX_SZ-__HAL_DMA_GET_COUNTER(COMM_UART_HANDLE.hdmarx));
    if(pos>=DMA_RX_SZ) pos=0u;
    if(pos==dma_last_pos) return;

    if(pos>dma_last_pos){
        uint16_t n=(uint16_t)(pos-dma_last_pos);
        ring_push(&dma_rx[dma_last_pos],n);
        raw_rx_bytes+=n;
    }else{
        uint16_t n1=(uint16_t)(DMA_RX_SZ-dma_last_pos);
        if(n1){ring_push(&dma_rx[dma_last_pos],n1);raw_rx_bytes+=n1;}
        if(pos){ring_push(&dma_rx[0],pos);raw_rx_bytes+=pos;}
    }
    dma_last_pos=pos;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef*h)
{
    if(h==&COMM_UART_HANDLE){
        uint32_t e=h->ErrorCode;
        uart_error_events++;
        if(e&HAL_UART_ERROR_ORE)uart_error_ore++;
        if(e&HAL_UART_ERROR_FE) uart_error_fe++;
        if(e&HAL_UART_ERROR_NE) uart_error_ne++;
        if(e&HAL_UART_ERROR_PE) uart_error_pe++;
        /* Do not perform a blocking abort/restart inside the IRQ callback. */
        rx_restart_pending=true;
    }else if(h==&huart1){
        DebugShell_OnUartError();
    }
}

void CommLink_Process(void)
{
    poll_dma_rx();

    uint8_t tmp[96];
    size_t n=0;
    while(ring_tail!=ring_head&&n<sizeof(tmp)){
        tmp[n++]=ring[ring_tail];
        ring_tail=(uint16_t)((ring_tail+1u)%RAW_RING_SZ);
    }
    if(!n)return;

    sa_frame_t f;
    bool got=sa_frame_parser_feed(&parser,tmp,n,&f);
    do{
        if(got&&f.cmd_id==SA_CMD_ROUTE){
            sa_route_packet_t r;
            if(sa_route_unpack(f.payload,f.payload_len,&r)&&route_cb)route_cb(&r);
        }
        got=sa_frame_parser_feed(&parser,0,0,&f);
    }while(got);
}

/* V2.4.5 TX policy
 * -----------------
 * V2.4.4 treated PB1/ESP READY as a mandatory hardware flow-control gate.
 * If READY was not wired, was wired to the wrong ESP pin, or stayed low during
 * boot, CommLink_SendRouteRaw() returned before USART1 ever transmitted.  RX
 * still worked, producing the misleading symptom "STM32 receives ESP but ESP
 * RX_FRAMES stays 0" on both MASTER and SLAVE because they share this code.
 *
 * The ESP firmware continuously services its business UART and drives READY
 * high unconditionally; WAKE is currently diagnostic only.  Therefore READY
 * cannot provide real back-pressure.  From V2.4.5 it is advisory: PB0 WAKE is
 * asserted for compatibility, PB1 is sampled for diagnostics, but a low READY
     * never suppresses UART TX.
 */
static void prepare_tx_link(void)
{
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);
    if(HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1)==GPIO_PIN_SET) tx_ready_high++;
    else tx_ready_low++;
}

bool CommLink_SendRouteRaw(const uint8_t*b,uint16_t l)
{
    if(!b||l>SA_MAX_PAYLOAD)return false;

    tx_attempts++;
    size_t n=sa_frame_encode(tx_seq++,SA_CMD_ROUTE,b,l,tx_buf,sizeof(tx_buf));
    if(!n){
        tx_encode_fail++;
        return false;
    }

    prepare_tx_link();

    /* USART1 TX is intentionally blocking here.  RX remains on independent
     * circular DMA, so full-duplex reception continues.  A SmartAgri
     * route is short enough that even the maximum frame is only a few ms at
     * 921600 baud.  This removes the DMA-TX callback/state dependency from the
     * critical STM32->ESP path and makes the return value deterministic. */
    HAL_StatusTypeDef st=HAL_UART_Transmit(&COMM_UART_HANDLE,tx_buf,(uint16_t)n,100u);
    HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_RESET);
    if(st!=HAL_OK){
        tx_hal_fail++;
        return false;
    }

    tx_ok_frames++;
    tx_bytes+=(uint32_t)n;
    return true;
}

bool CommLink_SendRoute(const sa_route_packet_t*r)
{
    if(!r)return false;
    uint16_t l=(uint16_t)(sizeof(r->h)+r->h.payload_len);
    return CommLink_SendRouteRaw((const uint8_t*)&r->h,l);
}

uint32_t CommLink_RxFrames(void){return parser.good_frames;}
uint32_t CommLink_RxDrops(void){return rx_drop+parser.resync_drop;}
uint32_t CommLink_RawRxBytes(void){return raw_rx_bytes;}
uint32_t CommLink_Crc8Fails(void){return parser.crc8_fail;}
uint32_t CommLink_Crc16Fails(void){return parser.crc16_fail;}
uint32_t CommLink_LengthFails(void){return parser.length_fail;}
uint32_t CommLink_UartErrorEvents(void){return uart_error_events;}
uint32_t CommLink_UartOverruns(void){return uart_error_ore;}
uint32_t CommLink_UartFrameErrors(void){return uart_error_fe;}
uint32_t CommLink_UartNoiseErrors(void){return uart_error_ne;}
uint32_t CommLink_UartParityErrors(void){return uart_error_pe;}
uint32_t CommLink_DmaRxRestarts(void){return dma_rx_restarts;}
uint32_t CommLink_DmaRxStartFails(void){return dma_rx_start_fail;}
uint32_t CommLink_DmaCndtr(void){return COMM_UART_HANDLE.hdmarx ? __HAL_DMA_GET_COUNTER(COMM_UART_HANDLE.hdmarx) : 0u;}
uint32_t CommLink_DmaLastPos(void){return dma_last_pos;}

void CommLink_PrintUart1Diag(void)
{
    DebugLog_Printf("[UART1-DIAG] instance=%s baud=%lu pclk2=%lu brr=0x%04lX cr1=0x%08lX cr3=0x%08lX sr=0x%08lX afio_mapr=0x%08lX dma=%s ccr=0x%08lX cndtr=%lu cpar=0x%08lX cmar=0x%08lX rxstate=%lu contract=%u\r\n",
                    (COMM_UART_HANDLE.Instance==COMM_USART) ? COMM_UART_NAME : "OTHER",
                    (unsigned long)COMM_UART_HANDLE.Init.BaudRate,
                    (unsigned long)((COMM_USART==USART1) ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq()),
                    (unsigned long)COMM_USART->BRR,
                    (unsigned long)COMM_USART->CR1,
                    (unsigned long)COMM_USART->CR3,
                    (unsigned long)COMM_USART->SR,
                    (unsigned long)AFIO->MAPR,
                    COMM_DMA_NAME,
                    (unsigned long)COMM_DMA_CH->CCR,
                    (unsigned long)COMM_DMA_CH->CNDTR,
                    (unsigned long)COMM_DMA_CH->CPAR,
                    (unsigned long)COMM_DMA_CH->CMAR,
                    (unsigned long)COMM_UART_HANDLE.RxState,
                    dma_contract_ok()?1u:0u);
}

uint32_t CommLink_TxAttempts(void){return tx_attempts;}
uint32_t CommLink_TxFrames(void){return tx_ok_frames;}
uint32_t CommLink_TxBytes(void){return tx_bytes;}
uint32_t CommLink_TxHalFails(void){return tx_hal_fail;}
uint32_t CommLink_TxEncodeFails(void){return tx_encode_fail;}
uint32_t CommLink_TxReadyHigh(void){return tx_ready_high;}
uint32_t CommLink_TxReadyLow(void){return tx_ready_low;}
