#include "board.h"
#include "debug_log.h"
#include "debug_shell.h"
#include "comm_link.h"
#include "pc_link.h"
#include "config_manager.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "node_service.h"

int main(void)
{
    SCB->VTOR = 0x08002000u;
    __DSB(); __ISB();
    /* Defensive reset-state restore: the OTA application must never depend on
     * the previous-stage PRIMASK value. HAL_Init() needs SysTick IRQs enabled. */
    __enable_irq();
    Board_Init();
    DebugLog_Init();

    DebugLog_Printf("\r\n[BOOT] SmartAgri STM32F103C8 V2.4.7 OTA App\r\n");
    DebugLog_Printf("[CLOCK] source=%s sys=%lu hclk=%lu pclk1=%lu pclk2=%lu HSE=%lu PLL=%lu attempts=%lu\r\n",
                    (g_board_clock_source == BOARD_CLOCK_HSE72) ? "HSE72" :
                    (g_board_clock_source == BOARD_CLOCK_HSI64) ? "HSI64" : "UNKNOWN",
                    (unsigned long)g_board_sysclk_hz,
                    (unsigned long)g_board_hclk_hz,
                    (unsigned long)g_board_pclk1_hz,
                    (unsigned long)g_board_pclk2_hz,
                    (unsigned long)g_board_hse_ready,
                    (unsigned long)g_board_pll_ready,
                    (unsigned long)g_board_hse_attempts);
    if (g_board_warning_code == BOARD_WARN_HSE_FALLBACK_HSI64) {
        DebugLog_Printf("[WARN] HSE 8MHz did not become ready; running diagnostic fallback HSI64. hse_status=%u\r\n",
                        (unsigned)g_board_hse_last_status);
    }

    DebugLog_Printf("[UART1] PC/Web A5 uplink + debug text 115200 PA9/PA10\r\n");
    DebugLog_Printf("[UART2] ESP link 921600 PA2/PA3 RX-DMA + robust blocking TX\r\n");
    DebugLog_Printf("[SWD] PA13/PA14 + NRST ST-LINK/OpenOCD\r\n");

    ConfigManager_Init();
    PcLink_Init();
    DebugShell_Init();
    const sa_node_config_t *c = ConfigManager_Get();
    DebugLog_Printf("[CFG-FALLBACK] role=%u device=%lu network=%u group=%u gen=%lu persisted=%u\r\n",
                    (unsigned)c->role,
                    (unsigned long)c->device_id,
                    (unsigned)c->network_id,
                    (unsigned)c->group_id,
                    (unsigned long)ConfigManager_Generation(),
                    ConfigManager_HasPersistedConfig()?1u:0u);

    CommLink_Init();
    CommLink_PrintUart1Diag();
    ActuatorManager_Init();
    SensorManager_Init(c);
    NodeService_Init();

    uint32_t last = 0;
    while (1) {
        PcLink_Process();
        DebugShell_Process();
        CommLink_Process();
        SensorManager_Process();
        ActuatorManager_Process();
        NodeService_Process();

        uint32_t now = Board_Millis();
        if (now - last >= 1000U) {
            last = now;
            DebugLog_Printf("[STAT] esp_raw=%lu esp_ok=%lu esp_drop=%lu crc8=%lu crc16=%lu uart_err=%lu ore=%lu fe=%lu ne=%lu dma_restart=%lu dma_startfail=%lu dma_cndtr=%lu dma_last_pos=%lu esp_tx_try=%lu esp_tx_ok=%lu esp_tx_bytes=%lu esp_tx_fail=%lu esp_tx_encfail=%lu ready_hi=%lu ready_lo=%lu pc_raw=%lu pc_ok=%lu pc_drop=%lu pc_tx=%lu\r\n",
                            (unsigned long)CommLink_RawRxBytes(),
                            (unsigned long)CommLink_RxFrames(),
                            (unsigned long)CommLink_RxDrops(),
                            (unsigned long)CommLink_Crc8Fails(),
                            (unsigned long)CommLink_Crc16Fails(),
                            (unsigned long)CommLink_UartErrorEvents(),
                            (unsigned long)CommLink_UartOverruns(),
                            (unsigned long)CommLink_UartFrameErrors(),
                            (unsigned long)CommLink_UartNoiseErrors(),
                            (unsigned long)CommLink_DmaRxRestarts(),
                            (unsigned long)CommLink_DmaRxStartFails(),
                            (unsigned long)CommLink_DmaCndtr(),
                            (unsigned long)CommLink_DmaLastPos(),
                            (unsigned long)CommLink_TxAttempts(),
                            (unsigned long)CommLink_TxFrames(),
                            (unsigned long)CommLink_TxBytes(),
                            (unsigned long)CommLink_TxHalFails(),
                            (unsigned long)CommLink_TxEncodeFails(),
                            (unsigned long)CommLink_TxReadyHigh(),
                            (unsigned long)CommLink_TxReadyLow(),
                            (unsigned long)PcLink_RawRxBytes(),
                            (unsigned long)PcLink_RxFrames(),
                            (unsigned long)PcLink_RxDrops(),
                            (unsigned long)PcLink_TxFrames());
        }
    }
}
