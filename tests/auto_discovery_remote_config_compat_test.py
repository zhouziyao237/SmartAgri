from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]

def text(rel):
    return (ROOT / rel).read_text(encoding='utf-8', errors='replace')

proto = text('ESP_Firmware/components/cluster_common/include/smartagri_protocol.h')
for tok in ('SA_CMD_RADIO_CONFIG_GET    = 0x3030', 'SA_CMD_RADIO_CONFIG_REPLY  = 0x3031',
            'SA_CMD_RADIO_CONFIG_SET    = 0x3032', 'SA_LINK_INFO_MAGIC', 'SA_PROTO_MIN_SUPPORTED',
            'SA_RADIO_CONFIG_SCHEMA'):
    assert tok in proto, tok

ac = text('ESP_Firmware/components/app_config/app_config.c')
assert 'SLAVE_GATEWAY_REQUIRED' not in ac
for tok in ('auto_discovery=1', 'channel_scan=1', 'CFG_FEATURE_MARKER'):
    assert tok in ac, tok

radio = text('ESP_Firmware/components/espnow_link/espnow_link.c')
assert 'else memcpy(dst,bcast,6);' in radio
assert 'learn(item.route.h.src_device,item.mac,item.rx_channel);' in radio
assert 'SA_CAP_AUTO_DISCOVERY' in radio

main = text('ESP_Firmware/main/app_main.c')
for tok in ('discovery_task', 'esp_wifi_set_channel(ch', 'bind_gateway_from_ack',
            'handle_radio_management', 'announce_radio_peer_to_pc',
            'cfg.role==ESP_ROLE_UNCONFIGURED', 'V2.4.8'):
    assert tok in main, tok

# V2.4.8: channel-race fix and stale-gateway recovery must be present.
for tok in ('info->rx_ctrl->channel', 'item.rx_channel=rx_channel',
            'espnow_link_get_peer_channel', 'uint8_t channel;uint32_t seen'):
    assert tok in radio, tok
for tok in ('GATEWAY_LOST_TIMEOUT_MS 15000u', 'source=ACK_RX',
            'recover_stale_gateway', 'start_discovery("gateway timeout")',
            'last_master_rx_tick=xTaskGetTickCount()', 'discovery_task_running'):
    assert tok in main, tok
# The old race source must not survive: binding may not persist whatever
# channel the scanner happens to be on when the deferred callback runs.
bind = main.split('static void bind_gateway_from_ack',1)[1].split('static void radio_to_uart',1)[0]
assert 'esp_wifi_get_channel(&ch,&sec)==ESP_OK)cfg.channel=ch' not in bind

web = text('PC_Tools/static/app.js')
assert '/api/radio/configure' in web and '/api/config/import' in web and '/api/export/' in web
qt = text('PC_Tools/qt_app/pages/radio.py')
assert '/api/radio/configure' in qt and '/api/config/import' in qt and '/api/export/' in qt

node = text('STM32_Firmware/app/node/node_service.c')
assert 'radio_targets_attached_esp' in node
assert 'Local MASTER recovery path' in node

sys.path.insert(0, str(ROOT / 'PC_Tools'))
import smartagri_protocol as p
sample = {
    'role': 2, 'device_id': 1001, 'network_id': 1, 'group_id': 1, 'channel': 6,
    'power_mode': 1, 'uplink': 0, 'auto_discovery': True, 'channel_scan': True,
    'gateway_valid': True, 'gateway_mac': 'C0:49:EF:F1:4B:18'
}
raw = p.pack_radio_config(sample)
assert len(raw) == p.RADIO_CONFIG_LEN
x = p.decode_radio_config(raw)
assert x['device_id'] == 1001 and x['gateway_mac'] == 'C0:49:EF:F1:4B:18'
assert x['proto_min'] == 2 and x['proto_max'] == 2
print('AUTO DISCOVERY / REMOTE CONFIG / COMPAT REGRESSION PASS')
