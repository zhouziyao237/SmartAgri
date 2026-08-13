from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ns = (ROOT / 'STM32_Firmware/app/node/node_service.c').read_text(encoding='utf-8')
pc = (ROOT / 'STM32_Firmware/app/comm/pc_link.c').read_text(encoding='utf-8')
main = (ROOT / 'STM32_Firmware/app/main.c').read_text(encoding='utf-8')

# Local PC link must no longer be globally blocked by identity_ready.
assert 'if(!r || !identity_ready)return;' not in ns
assert 'persisted_master_local_pc_available' in ns
assert 'local_pc_available' in ns
assert 'if(!identity_ready){' in ns

# WAIT_ESP branch must service local commands but must not forward to radio.
wait = ns.split('if(!identity_ready){', 1)[1].split('if(r->h.dst_device==SA_DEVICE_BROADCAST)', 1)[0]
# The actual branch below contains local processing and returns before normal forwarding.
handler = ns.split('static void on_route_from_pc', 1)[1].split('void NodeService_Init', 1)[0]
assert 'if(local_dst(r) && !ota_targets_attached_esp(r)) process_local(r);' in handler
assert 'LOCAL MASTER management alive' in handler

# Egress policy must permit PC only, while all radio egress remains gated.
assert 'if(dst==SA_DEVICE_PC && local_pc_available(c)) {' in ns
assert 'bool pc_ok = PcLink_SendRouteRaw(b,l);' in ns
assert 'if(identity_ready) (void)CommLink_SendRouteRaw(b,l);' in ns
assert 'if(!identity_ready) return false;' in ns

# Add raw USART2 diagnostics so a future failure can distinguish no bytes vs parser failure.
for token in ('g_raw_rx_bytes', 'PcLink_RawRxBytes', 'PcLink_Crc8Fails',
              'PcLink_Crc16Fails', 'PcLink_UartErrorEvents'):
    assert token in pc or token in main, token
assert 'pc_raw=%lu pc_ok=%lu' in main

print('PC LOCAL LINK REGRESSION PASS')
