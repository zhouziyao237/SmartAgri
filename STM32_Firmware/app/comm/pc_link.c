#include "pc_link.h"
#include "board.h"
#include <string.h>

#define PC_RX_RING_SZ 1024u

static volatile uint16_t g_head;
static volatile uint16_t g_tail;
static uint8_t g_ring[PC_RX_RING_SZ];
static volatile uint32_t g_ring_drop;

/* Lightweight ISR-side frame boundary tracker.  It only decides whether a byte
 * belongs to an A5 frame. CRC/semantic validation is deliberately deferred to
 * PcLink_Process() in main context.
 */
static volatile bool g_in_frame;
static volatile uint16_t g_frame_count;
static volatile uint16_t g_frame_expected;
static uint8_t g_prefix[3];

static sa_frame_parser_t g_parser;
static pc_route_cb_t g_route_cb;
static uint8_t g_tx_seq;
static uint8_t g_tx_buf[SA_MAX_FRAME];
static uint32_t g_tx_frames;
static volatile uint32_t g_raw_rx_bytes;
static volatile uint32_t g_uart_error_events;

static void ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((g_head + 1u) % PC_RX_RING_SZ);
    if (next == g_tail) {
        g_ring_drop++;
        return;
    }
    g_ring[g_head] = b;
    g_head = next;
}

void PcLink_Init(void)
{
    g_head = g_tail = 0u;
    g_ring_drop = 0u;
    g_in_frame = false;
    g_frame_count = 0u;
    g_frame_expected = 0u;
    g_route_cb = 0;
    g_tx_seq = 0u;
    g_tx_frames = 0u;
    g_raw_rx_bytes = 0u;
    g_uart_error_events = 0u;
    sa_frame_parser_init(&g_parser);
}

void PcLink_SetRouteCallback(pc_route_cb_t cb) { g_route_cb = cb; }

bool PcLink_ConsumeRxByte(uint8_t b)
{
    g_raw_rx_bytes++;
    if (!g_in_frame) {
        if (b != SA_SOF) return false;
        g_in_frame = true;
        g_frame_count = 0u;
        g_frame_expected = 0u;
    }

    if (g_frame_count < 3u) g_prefix[g_frame_count] = b;
    ring_push(b);
    g_frame_count++;

    if (g_frame_count == 3u) {
        uint16_t payload_len = (uint16_t)g_prefix[1] | ((uint16_t)g_prefix[2] << 8);
        if (payload_len > SA_MAX_PAYLOAD) {
            /* Bad length.  Let the main parser account for the malformed data,
             * then immediately return to text/binary auto-detection.
             */
            g_in_frame = false;
            g_frame_count = 0u;
            g_frame_expected = 0u;
        } else {
            g_frame_expected = (uint16_t)(SA_FRAME_OVERHEAD + payload_len);
        }
    }

    if (g_in_frame && g_frame_expected != 0u && g_frame_count >= g_frame_expected) {
        g_in_frame = false;
        g_frame_count = 0u;
        g_frame_expected = 0u;
    }
    return true;
}

void PcLink_OnUartError(void)
{
    g_uart_error_events++;
    g_in_frame = false;
    g_frame_count = 0u;
    g_frame_expected = 0u;
}

void PcLink_Process(void)
{
    uint8_t tmp[96];
    size_t n = 0u;
    while (g_tail != g_head && n < sizeof(tmp)) {
        tmp[n++] = g_ring[g_tail];
        g_tail = (uint16_t)((g_tail + 1u) % PC_RX_RING_SZ);
    }
    if (!n) return;

    sa_frame_t f;
    bool got = sa_frame_parser_feed(&g_parser, tmp, n, &f);
    do {
        if (got && f.cmd_id == SA_CMD_ROUTE) {
            sa_route_packet_t r;
            if (sa_route_unpack(f.payload, f.payload_len, &r) && g_route_cb) {
                g_route_cb(&r);
            }
        }
        got = sa_frame_parser_feed(&g_parser, 0, 0, &f);
    } while (got);
}

bool PcLink_SendRouteRaw(const uint8_t *route_bytes, uint16_t route_len)
{
    if (!route_bytes || route_len > SA_MAX_PAYLOAD) return false;
    size_t n = sa_frame_encode(g_tx_seq++, SA_CMD_ROUTE, route_bytes, route_len,
                               g_tx_buf, sizeof(g_tx_buf));
    if (!n) return false;
    if (HAL_UART_Transmit(&huart1, g_tx_buf, (uint16_t)n, 250u) != HAL_OK) return false;
    g_tx_frames++;
    return true;
}

bool PcLink_SendRoute(const sa_route_packet_t *r)
{
    if (!r || r->h.payload_len > SA_MAX_ROUTE_PAYLOAD) return false;
    uint16_t len = (uint16_t)(sizeof(r->h) + r->h.payload_len);
    return PcLink_SendRouteRaw((const uint8_t *)&r->h, len);
}

uint32_t PcLink_RxFrames(void) { return g_parser.good_frames; }
uint32_t PcLink_RxDrops(void) { return g_ring_drop + g_parser.resync_drop; }
uint32_t PcLink_TxFrames(void) { return g_tx_frames; }
uint32_t PcLink_RawRxBytes(void) { return g_raw_rx_bytes; }
uint32_t PcLink_Crc8Fails(void) { return g_parser.crc8_fail; }
uint32_t PcLink_Crc16Fails(void) { return g_parser.crc16_fail; }
uint32_t PcLink_LengthFails(void) { return g_parser.length_fail; }
uint32_t PcLink_UartErrorEvents(void) { return g_uart_error_events; }
