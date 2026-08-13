#include "debug_console.h"
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#if CONFIG_IDF_TARGET_ESP32C3
#define DEBUG_TRANSPORT "UART0"
#define DEBUG_TX_PIN    21
#define DEBUG_RX_PIN    20
#elif CONFIG_IDF_TARGET_ESP32
#define DEBUG_TRANSPORT "UART0"
#define DEBUG_TX_PIN    1
#define DEBUG_RX_PIN    3
#else
#error "SmartAgri V2.4.8 supports IDF_TARGET=esp32c3 or esp32"
#endif

#define DEBUG_BAUD 115200

static debug_console_line_cb_t g_line_cb;
static SemaphoreHandle_t g_tx_mutex;

const char *debug_console_transport_name(void) { return DEBUG_TRANSPORT; }
int debug_console_baud(void) { return DEBUG_BAUD; }
int debug_console_tx_pin(void) { return DEBUG_TX_PIN; }
int debug_console_rx_pin(void) { return DEBUG_RX_PIN; }

void debug_console_write_text(const char *text)
{
    if(!text || !*text) return;
    if(g_tx_mutex) xSemaphoreTake(g_tx_mutex, portMAX_DELAY);
    fputs(text, stdout);
    fflush(stdout);
    if(g_tx_mutex) xSemaphoreGive(g_tx_mutex);
}

static void debug_console_task(void *arg)
{
    (void)arg;
    char line[256];
    size_t line_len = 0;
    uint8_t rx[64];

    /* ESP-IDF primary UART0 console carries both ESP_LOG and interactive AT.
     * This task is dedicated to RX, so a blocking console read is acceptable. */
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    for(;;) {
        ssize_t n = read(STDIN_FILENO, rx, sizeof(rx));
        if(n <= 0) {
            if(n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        for(ssize_t i = 0; i < n; ++i) {
            uint8_t c = rx[i];
            if(c == '\r' || c == '\n') {
                if(line_len > 0) {
                    line[line_len] = '\0';
                    if(g_line_cb) g_line_cb(line);
                    line_len = 0;
                }
                continue;
            }

            if(c == 0x08 || c == 0x7F) {
                if(line_len > 0) --line_len;
                continue;
            }

            if(c >= 0x20 && c <= 0x7E) {
                if(line_len < sizeof(line) - 1) {
                    line[line_len++] = (char)c;
                } else {
                    line_len = 0;
                    debug_console_write_text("+AT:LINE_TOO_LONG\r\nERROR\r\n");
                }
            }
        }
    }
}

void debug_console_init(debug_console_line_cb_t line_cb)
{
    g_line_cb = line_cb;
    g_tx_mutex = xSemaphoreCreateMutex();
    xTaskCreate(debug_console_task, "debug_at", 4096, NULL, 10, NULL);
}
