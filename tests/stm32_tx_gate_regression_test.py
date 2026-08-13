from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
comm=(ROOT/'STM32_Firmware/app/comm/comm_link.c').read_text(encoding='utf-8')
main=(ROOT/'STM32_Firmware/app/main.c').read_text(encoding='utf-8')
node=(ROOT/'STM32_Firmware/app/node/node_service.c').read_text(encoding='utf-8')
boot=(ROOT/'STM32_Firmware/bootloader/bootloader.c').read_text(encoding='utf-8')

# V2.4.4 hard READY gate must be gone. READY is diagnostic only.
assert 'if(!wait_ready(50))return false;' not in comm
assert 'static void prepare_tx_link(void)' in comm
assert 'tx_ready_low++' in comm
assert 'HAL_UART_Transmit(&COMM_UART_HANDLE' in comm
assert 'HAL_UART_Transmit_DMA(&COMM_UART_HANDLE' not in comm

# Explicit TX counters must be visible in the 1 Hz diagnostics.
for tok in ('CommLink_TxAttempts','CommLink_TxFrames','CommLink_TxBytes',
            'CommLink_TxHalFails','CommLink_TxReadyHigh','CommLink_TxReadyLow'):
    assert tok in comm and tok in main, tok
assert 'esp_tx_try=%lu esp_tx_ok=%lu esp_tx_bytes=%lu esp_tx_fail=%lu' in main
assert 'ready_hi=%lu ready_lo=%lu' in main

# OTA/app version bump.
assert '0x00020407u' in node
assert '0x00020407u' in boot
assert 'V2.4.7 OTA App' in main
print('STM32 TX GATE REGRESSION PASS')
