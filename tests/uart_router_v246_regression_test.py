from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def text(rel): return (ROOT/rel).read_text(encoding='utf-8',errors='replace')

node=text('STM32_Firmware/app/node/node_service.c')
assert 'ESP_IDENTITY_LOSS_MS 7000u' in node
assert '(void)send_node_info(SA_DEVICE_LOCAL_ESP);' in node
assert 'if(identity_changed) (void)send_node_info(SA_DEVICE_PC);' in node
assert '(identity_ready || local_pc_available(c))' in node

uart=text('ESP_Firmware/components/uart_bridge/uart_bridge.c')
for tok in ('uart_write_bytes','written != (int)frame_len','uart_wait_tx_done','g_tx_write_fail','g_tx_wait_fail','g_rx_raw_bytes','UART_FRAME_ERR','UART_FIFO_OVF'):
    assert tok in uart, tok
assert 'g_tx_frames++;\n    return true;' not in uart, 'old unconditional TX success pattern survived'

main=text('ESP_Firmware/main/app_main.c')
assert 'consume_local_stm32_route' in main
assert 'if(!radio_ready) return;' in main
assert 'src_device==SA_DEVICE_LOCAL_ESP' in main

radio=text('ESP_Firmware/components/espnow_link/espnow_link.c')
assert 'xQueueSend(deliver_q' in radio
assert 'delivery_task' in radio
assert 'dst_device==SA_DEVICE_LOCAL_ESP' in radio

at=text('ESP_Firmware/components/at_shell/at_shell.c')
for tok in ('TX_WRITE_FAIL','TX_WAIT_FAIL','RX_RAW','CRC8','FRAME_ERR'):
    assert tok in at

# Protocol command enum must be synchronized for the diagnostic command too.
sh=text('STM32_Firmware/app/protocol/smartagri_protocol.h')
eh=text('ESP_Firmware/components/cluster_common/include/smartagri_protocol.h')
assert 'SA_CMD_DIAG_TELEMETRY      = 0x31F0' in sh and 'SA_CMD_DIAG_TELEMETRY      = 0x31F0' in eh

assert 'if(!local_group_match(r)) return;' in node
assert 'if(identity_ready) (void)CommLink_SendRouteRaw(b,l);' in node
assert 'if(transfer==0u)' in node
assert 'item.route.h.group_id!=cfg.group_id' in radio
assert 'cfg.uplink==ESP_UPLINK_TCP' in main and 'tcp_uplink_send_route(r)' in main
ota=text('ESP_Firmware/components/ota_manager/ota_manager.c')
assert 'g_state!=SA_OTA_STATE_READY' in ota
assert 'b->transfer_id==0u' in ota
assert 'esp_ota_abort(g_handle);g_active=false;' in ota
cm=text('ESP_Firmware/CMakeLists.txt')
assert 'project(smartagri_esp_dualtarget_cluster_v2_4_6)' in cm
for rel in ('ESP_Firmware/scripts/windows/build_flash_esp32.ps1','ESP_Firmware/scripts/windows/build_flash_c3.ps1','ESP_Firmware/scripts/windows/build_ota_app.ps1'):
    sc=text(rel)
    assert 'v2_4_1.bin' not in sc
    assert 'V2.4.2.bin' not in sc

# Concurrent ESP UART producers must serialize attempt/sequence accounting with
# the actual write, rather than incrementing diagnostics before the mutex.
assert uart.index('xSemaphoreTake(g_tx_mutex') < uart.index('g_tx_attempts++') < uart.index('sa_frame_encode(g_seq++')

# OTA write failure must close the active ESP OTA handle, not leave a poisoned
# transaction that can continue accepting DATA.
write_fail = ota.split('esp_ota_write(g_handle,d,h->data_len)',1)[1].split('g_crc_state=',1)[0]
assert 'esp_ota_abort(g_handle)' in write_fail and 'g_active=false' in write_fail

# STM32 bootloader must reject transfer_id=0 on BEGIN as well as PREPARE.
boot=text('STM32_Firmware/bootloader/bootloader.c')
begin=boot.split('if(cmd==SA_CMD_OTA_BEGIN&&plen==16)',1)[1].split('if(cmd==SA_CMD_OTA_DATA',1)[0]
assert 'if(!tid)' in begin

# Debug text must not transmit the trailing NUL after vsnprintf truncation.
dbg=text('STM32_Firmware/app/log/debug_log.c')
assert 'n>=(int)sizeof(g_log_buf)' in dbg and 'sizeof(g_log_buf)-1' in dbg

print('UART/ROUTER V2.4.6 REGRESSION PASS')
