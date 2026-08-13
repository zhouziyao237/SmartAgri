#include "debug_shell.h"
#include "debug_log.h"
#include "board.h"
#include "config_manager.h"
#include "comm_link.h"
#include "sensor_manager.h"
#include "pc_link.h"
#include "node_service.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

#define DBG_RX_RING_SIZE 128u
#define DBG_LINE_SIZE     96u

static volatile uint8_t g_rx_byte;
static volatile uint16_t g_rx_head;
static volatile uint16_t g_rx_tail;
static uint8_t g_rx_ring[DBG_RX_RING_SIZE];
static char g_line[DBG_LINE_SIZE];
static uint16_t g_line_len;
static volatile uint32_t g_rx_bytes;
static volatile uint32_t g_rx_overflow;

static void rearm_rx(void)
{
    (void)HAL_UART_Receive_IT(&huart1, (uint8_t *)&g_rx_byte, 1u);
}

static void ring_push(uint8_t b)
{
    uint16_t next = (uint16_t)((g_rx_head + 1u) % DBG_RX_RING_SIZE);
    if (next == g_rx_tail) {
        g_rx_overflow++;
        return;
    }
    g_rx_ring[g_rx_head] = b;
    g_rx_head = next;
    g_rx_bytes++;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *h)
{
    if (h == &huart1) {
        /* A5 binary frames belong to the formal PC/Web link.  Plain printable
         * text remains available to the engineering debug shell. */
        if (!PcLink_ConsumeRxByte(g_rx_byte)) ring_push(g_rx_byte);
        rearm_rx();
    }
}

void DebugShell_Init(void)
{
    g_rx_head = 0u;
    g_rx_tail = 0u;
    g_line_len = 0u;
    g_rx_bytes = 0u;
    g_rx_overflow = 0u;
    rearm_rx();
}

void DebugShell_OnUartError(void)
{
    PcLink_OnUartError();
    (void)HAL_UART_AbortReceive(&huart1);
    rearm_rx();
}

uint32_t DebugShell_RxBytes(void) { return g_rx_bytes; }
uint32_t DebugShell_RxOverflow(void) { return g_rx_overflow; }

static const char *role_name(uint8_t role)
{
    switch (role) {
        case SA_ROLE_MASTER: return "MASTER";
        case SA_ROLE_SLAVE: return "SLAVE";
        default: return "UNCONFIGURED";
    }
}

static void print_clock(void)
{
    DebugLog_Printf("clock.source=%s\r\n",
                    (g_board_clock_source == BOARD_CLOCK_HSE72) ? "HSE72" :
                    (g_board_clock_source == BOARD_CLOCK_HSI64) ? "HSI64" : "UNKNOWN");
    DebugLog_Printf("clock.sys=%lu hclk=%lu pclk1=%lu pclk2=%lu\r\n",
                    (unsigned long)g_board_sysclk_hz,
                    (unsigned long)g_board_hclk_hz,
                    (unsigned long)g_board_pclk1_hz,
                    (unsigned long)g_board_pclk2_hz);
    DebugLog_Printf("clock.hse_on=%lu hse_ready=%lu pll_on=%lu pll_ready=%lu attempts=%lu\r\n",
                    (unsigned long)g_board_hse_on,
                    (unsigned long)g_board_hse_ready,
                    (unsigned long)g_board_pll_on,
                    (unsigned long)g_board_pll_ready,
                    (unsigned long)g_board_hse_attempts);
    DebugLog_Printf("clock.warn=0x%04lX hse_status=%u\r\n",
                    (unsigned long)g_board_warning_code,
                    (unsigned)g_board_hse_last_status);
}

static void print_config(void)
{
    const sa_node_config_t *c = ConfigManager_Get();
    DebugLog_Printf("config.role=%s(%u) device=%lu network=%u group=%u gen=%lu sensors=%u\r\n",
                    role_name(c->role), (unsigned)c->role,
                    (unsigned long)c->device_id,
                    (unsigned)c->network_id,
                    (unsigned)c->group_id,
                    (unsigned long)ConfigManager_Generation(),
                    (unsigned)c->sensor_count);
    DebugLog_Printf("identity.ready=%u source=%s persisted=%u tx_open=%u last_esp_ms=%lu\r\n",
                    NodeService_IdentityReady()?1u:0u,
                    NodeService_IdentityFromEsp()?"ESP":"WAIT_ESP",
                    ConfigManager_HasPersistedConfig()?1u:0u,
                    ConfigManager_TransactionActive()?1u:0u,
                    (unsigned long)NodeService_LastEspIdentityMs());
}

static void print_sensors(void)
{
    const sa_node_config_t *c = ConfigManager_Get();
    uint32_t shown = 0u;
    for (uint32_t i = 0u; i < SA_MAX_SENSOR_SLOTS; ++i) {
        const sa_sensor_slot_config_t *s = &c->slots[i];
        if (!s->enabled || s->driver_id == SA_DRV_NONE) continue;
        DebugLog_Printf("slot=%u drv=%u(%s) if=%u pin=%u addr=0x%02X sample=%lu report=%lu\r\n",
                        (unsigned)s->slot_id,
                        (unsigned)s->driver_id,
                        SensorManager_DriverName(s->driver_id),
                        (unsigned)s->interface_type,
                        (unsigned)s->pin_id,
                        (unsigned)s->address,
                        (unsigned long)s->sample_interval_ms,
                        (unsigned long)s->report_interval_ms);
        shown++;
    }
    if (shown == 0u) DebugLog_Printf("sensors: none\r\n");
}

static void print_status(void)
{
    print_config();
    DebugLog_Printf("esp.rx=%lu esp.drop=%lu pc.raw=%lu pc.ok=%lu pc.drop=%lu pc.tx=%lu pc.uart_err=%lu dbg.rx_bytes=%lu dbg.overflow=%lu\r\n",
                    (unsigned long)CommLink_RxFrames(),
                    (unsigned long)CommLink_RxDrops(),
                    (unsigned long)PcLink_RawRxBytes(),
                    (unsigned long)PcLink_RxFrames(),
                    (unsigned long)PcLink_RxDrops(),
                    (unsigned long)PcLink_TxFrames(),
                    (unsigned long)PcLink_UartErrorEvents(),
                    (unsigned long)DebugShell_RxBytes(),
                    (unsigned long)DebugShell_RxOverflow());
    print_clock();
}

static void print_help(void)
{
    DebugLog_Printf("commands:\r\n");
    DebugLog_Printf("  help      - show this help\r\n");
    DebugLog_Printf("  status    - node/comm/clock status\r\n");
    DebugLog_Printf("  clock     - RCC clock diagnostics\r\n");
    DebugLog_Printf("  config    - active node configuration\r\n");
    DebugLog_Printf("  stats     - UART1/USART2 receive counters\r\n");
    DebugLog_Printf("  sensors   - configured sensor slots\r\n");
    DebugLog_Printf("  reboot    - NVIC system reset\r\n");
}

static void execute_line(char *line)
{
    while (*line && isspace((unsigned char)*line)) line++;
    char *end = line + strlen(line);
    while (end > line && isspace((unsigned char)end[-1])) *--end = '\0';
    for (char *p = line; *p; ++p) *p = (char)tolower((unsigned char)*p);

    if (*line == '\0') return;
    if (strcmp(line, "help") == 0 || strcmp(line, "?") == 0) {
        print_help();
    } else if (strcmp(line, "status") == 0) {
        print_status();
    } else if (strcmp(line, "clock") == 0) {
        print_clock();
    } else if (strcmp(line, "config") == 0) {
        print_config();
    } else if (strcmp(line, "stats") == 0) {
        DebugLog_Printf("esp.raw=%lu esp.ok=%lu esp.drop=%lu crc8=%lu crc16=%lu lenfail=%lu uart.err=%lu ore=%lu fe=%lu ne=%lu pe=%lu dma.restart=%lu dma.startfail=%lu dma.cndtr=%lu dma.last_pos=%lu pc.raw=%lu pc.ok=%lu pc.drop=%lu pc.crc8=%lu pc.crc16=%lu pc.lenfail=%lu pc.uart_err=%lu pc.tx=%lu dbg.rx_bytes=%lu dbg.overflow=%lu\r\n",
                        (unsigned long)CommLink_RawRxBytes(),
                        (unsigned long)CommLink_RxFrames(),
                        (unsigned long)CommLink_RxDrops(),
                        (unsigned long)CommLink_Crc8Fails(),
                        (unsigned long)CommLink_Crc16Fails(),
                        (unsigned long)CommLink_LengthFails(),
                        (unsigned long)CommLink_UartErrorEvents(),
                        (unsigned long)CommLink_UartOverruns(),
                        (unsigned long)CommLink_UartFrameErrors(),
                        (unsigned long)CommLink_UartNoiseErrors(),
                        (unsigned long)CommLink_UartParityErrors(),
                        (unsigned long)CommLink_DmaRxRestarts(),
                        (unsigned long)CommLink_DmaRxStartFails(),
                        (unsigned long)CommLink_DmaCndtr(),
                        (unsigned long)CommLink_DmaLastPos(),
                        (unsigned long)PcLink_RawRxBytes(),
                        (unsigned long)PcLink_RxFrames(),
                        (unsigned long)PcLink_RxDrops(),
                        (unsigned long)PcLink_Crc8Fails(),
                        (unsigned long)PcLink_Crc16Fails(),
                        (unsigned long)PcLink_LengthFails(),
                        (unsigned long)PcLink_UartErrorEvents(),
                        (unsigned long)PcLink_TxFrames(),
                        (unsigned long)DebugShell_RxBytes(),
                        (unsigned long)DebugShell_RxOverflow());
    } else if (strcmp(line, "sensors") == 0) {
        print_sensors();
    } else if (strcmp(line, "reboot") == 0) {
        DebugLog_Printf("rebooting...\r\n");
        HAL_Delay(20u);
        NVIC_SystemReset();
    } else {
        DebugLog_Printf("ERR unknown command: %s (type help)\r\n", line);
    }
}

void DebugShell_Process(void)
{
    while (g_rx_tail != g_rx_head) {
        uint8_t b = g_rx_ring[g_rx_tail];
        g_rx_tail = (uint16_t)((g_rx_tail + 1u) % DBG_RX_RING_SIZE);

        if (b == '\r' || b == '\n') {
            if (g_line_len != 0u) {
                g_line[g_line_len] = '\0';
                execute_line(g_line);
                g_line_len = 0u;
            }
            continue;
        }

        if (b == 0x08u || b == 0x7Fu) {
            if (g_line_len != 0u) g_line_len--;
            continue;
        }

        if (b >= 0x20u && b <= 0x7Eu) {
            if (g_line_len < (DBG_LINE_SIZE - 1u)) {
                g_line[g_line_len++] = (char)b;
            } else {
                g_line_len = 0u;
                DebugLog_Printf("ERR debug line too long\r\n");
            }
        }
    }
}
