#include "uart_bridge.h"
#include "sdkconfig.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#if CONFIG_IDF_TARGET_ESP32C3
#define BUS_UART   UART_NUM_1
#define TX_PIN     7
#define RX_PIN     6
#define WAKE_PIN   3
#define READY_PIN  10
#define TARGET_NAME "ESP32-C3"
#elif CONFIG_IDF_TARGET_ESP32
/* ESP32-WROOM / common DevKit defaults. ESP32-WROVER users may need to
 * remap GPIO16/17 if those pins are occupied by PSRAM. */
#define BUS_UART   UART_NUM_2
#define TX_PIN     17
#define RX_PIN     16
#define WAKE_PIN   25
#define READY_PIN  26
#define TARGET_NAME "ESP32"
#else
#error "SmartAgri supports IDF_TARGET=esp32c3 or esp32"
#endif

#define BUS_BAUD 921600
#define UART_EVENT_QUEUE_LEN 20

static uart_route_cb_t g_route_cb;
static SemaphoreHandle_t g_tx_mutex;
static QueueHandle_t g_uart_queue;
static sa_frame_parser_t g_parser;
static uint8_t g_seq;

static volatile uint32_t g_tx_attempts;
static volatile uint32_t g_tx_frames;
static volatile uint32_t g_tx_bytes;
static volatile uint32_t g_tx_write_fail;
static volatile uint32_t g_tx_wait_fail;
static volatile uint32_t g_tx_encode_fail;
static volatile uint32_t g_rx_raw_bytes;
static volatile uint32_t g_rx_frames;
static volatile uint32_t g_rx_route_drops;
static volatile uint32_t g_rx_fifo_ovf;
static volatile uint32_t g_rx_buffer_full;
static volatile uint32_t g_rx_frame_err;
static volatile uint32_t g_rx_parity_err;

const char *uart_bridge_target_name(void) { return TARGET_NAME; }
int uart_bridge_uart_num(void) { return (int)BUS_UART; }
int uart_bridge_tx_pin(void) { return TX_PIN; }
int uart_bridge_rx_pin(void) { return RX_PIN; }
int uart_bridge_wake_pin(void) { return WAKE_PIN; }
int uart_bridge_ready_pin(void) { return READY_PIN; }
int uart_bridge_baud(void) { return BUS_BAUD; }
uint32_t uart_bridge_tx_attempts(void) { return g_tx_attempts; }
uint32_t uart_bridge_tx_frames(void) { return g_tx_frames; }
uint32_t uart_bridge_tx_bytes(void) { return g_tx_bytes; }
uint32_t uart_bridge_tx_write_fail(void) { return g_tx_write_fail; }
uint32_t uart_bridge_tx_wait_fail(void) { return g_tx_wait_fail; }
uint32_t uart_bridge_tx_encode_fail(void) { return g_tx_encode_fail; }
uint32_t uart_bridge_rx_raw_bytes(void) { return g_rx_raw_bytes; }
uint32_t uart_bridge_rx_frames(void) { return g_rx_frames; }
uint32_t uart_bridge_rx_parse_drops(void) { return g_rx_route_drops + g_parser.resync_drop; }
uint32_t uart_bridge_rx_crc8_fail(void) { return g_parser.crc8_fail; }
uint32_t uart_bridge_rx_crc16_fail(void) { return g_parser.crc16_fail; }
uint32_t uart_bridge_rx_length_fail(void) { return g_parser.length_fail; }
uint32_t uart_bridge_rx_fifo_ovf(void) { return g_rx_fifo_ovf; }
uint32_t uart_bridge_rx_buffer_full(void) { return g_rx_buffer_full; }
uint32_t uart_bridge_rx_frame_err(void) { return g_rx_frame_err; }
uint32_t uart_bridge_rx_parity_err(void) { return g_rx_parity_err; }

bool uart_bridge_send_route(const uint8_t *route, uint16_t len)
{
    if(!g_tx_mutex || !route || len > SA_MAX_PAYLOAD) return false;

    if(xSemaphoreTake(g_tx_mutex, portMAX_DELAY) != pdTRUE) {
        g_tx_write_fail++;
        return false;
    }
    /* Serialize the diagnostic counters and sequence number with the actual
     * UART write. Multiple producers (identity, ESP-NOW router, OTA/TCP) may
     * call this function from different FreeRTOS tasks. */
    g_tx_attempts++;

    uint8_t frame[SA_MAX_FRAME];
    size_t frame_len = sa_frame_encode(g_seq++, SA_CMD_ROUTE, route, len,
                                       frame, sizeof(frame));
    if(frame_len == 0) {
        g_tx_encode_fail++;
        xSemaphoreGive(g_tx_mutex);
        return false;
    }

    int written = uart_write_bytes(BUS_UART, frame, frame_len);
    if(written != (int)frame_len) {
        g_tx_write_fail++;
        xSemaphoreGive(g_tx_mutex);
        return false;
    }

    if(uart_wait_tx_done(BUS_UART, pdMS_TO_TICKS(100)) != ESP_OK) {
        g_tx_wait_fail++;
        xSemaphoreGive(g_tx_mutex);
        return false;
    }

    g_tx_frames++;
    g_tx_bytes += (uint32_t)frame_len;
    xSemaphoreGive(g_tx_mutex);
    return true;
}

static void process_rx_bytes(const uint8_t *rx, int n)
{
    if(!rx || n <= 0) return;
    g_rx_raw_bytes += (uint32_t)n;

    sa_frame_t frame;
    bool got = sa_frame_parser_feed(&g_parser, rx, (size_t)n, &frame);
    do {
        if(got && frame.cmd_id == SA_CMD_ROUTE) {
            sa_route_packet_t route;
            if(sa_route_unpack(frame.payload, frame.payload_len, &route)) {
                g_rx_frames++;
                if(g_route_cb) g_route_cb(&route);
            } else {
                g_rx_route_drops++;
            }
        }
        got = sa_frame_parser_feed(&g_parser, NULL, 0, &frame);
    } while(got);
}

static void business_uart_task(void *arg)
{
    (void)arg;
    uint8_t rx[192];
    uart_event_t event;

    for(;;) {
        if(xQueueReceive(g_uart_queue, &event, portMAX_DELAY) != pdTRUE) continue;
        switch(event.type) {
            case UART_DATA: {
                size_t remain = event.size;
                while(remain > 0u) {
                    size_t want = remain > sizeof(rx) ? sizeof(rx) : remain;
                    int n = uart_read_bytes(BUS_UART, rx, want, pdMS_TO_TICKS(30));
                    if(n <= 0) break;
                    process_rx_bytes(rx, n);
                    remain -= (size_t)n;
                }
                break;
            }
            case UART_FIFO_OVF:
                g_rx_fifo_ovf++;
                uart_flush_input(BUS_UART);
                xQueueReset(g_uart_queue);
                sa_frame_parser_init(&g_parser);
                break;
            case UART_BUFFER_FULL:
                g_rx_buffer_full++;
                uart_flush_input(BUS_UART);
                xQueueReset(g_uart_queue);
                sa_frame_parser_init(&g_parser);
                break;
            case UART_FRAME_ERR:
                g_rx_frame_err++;
                break;
            case UART_PARITY_ERR:
                g_rx_parity_err++;
                break;
            default:
                break;
        }
    }
}

void uart_bridge_init(uart_route_cb_t route_cb)
{
    g_route_cb = route_cb;
    g_tx_mutex = xSemaphoreCreateMutex();
    g_uart_queue = NULL;
    g_seq = 0;
    g_tx_attempts = g_tx_frames = g_tx_bytes = 0;
    g_tx_write_fail = g_tx_wait_fail = g_tx_encode_fail = 0;
    g_rx_raw_bytes = g_rx_frames = g_rx_route_drops = 0;
    g_rx_fifo_ovf = g_rx_buffer_full = g_rx_frame_err = g_rx_parity_err = 0;
    sa_frame_parser_init(&g_parser);

    uart_config_t cfg = {
        .baud_rate = BUS_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(uart_param_config(BUS_UART, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(BUS_UART, TX_PIN, RX_PIN,
                                 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(BUS_UART, 2048, 0, UART_EVENT_QUEUE_LEN,
                                        &g_uart_queue, 0));

    gpio_config_t gpio_cfg = {
        .pin_bit_mask = (1ULL << WAKE_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));

    gpio_cfg.pin_bit_mask = (1ULL << READY_PIN);
    gpio_cfg.mode = GPIO_MODE_OUTPUT;
    gpio_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio_cfg));
    gpio_set_level(READY_PIN, 1);

    configASSERT(g_tx_mutex != NULL);
    configASSERT(g_uart_queue != NULL);
    BaseType_t ok = xTaskCreate(business_uart_task, "stm32_bus_uart", 4096, NULL, 12, NULL);
    configASSERT(ok == pdPASS);
}
