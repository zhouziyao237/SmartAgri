from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

def need(path, *tokens):
    text = (ROOT / path).read_text(encoding='utf-8', errors='replace')
    for token in tokens:
        assert token in text, f'{path}: missing {token!r}'
    return text

cm = need('STM32_Firmware/app/config/config_manager.c',
          'c->role=SA_ROLE_UNCONFIGURED;',
          'copy_identity(&candidate,&active_cfg);',
          'if(transaction_active)',
          'ConfigManager_FactoryReset',
          'identity_values_valid')
assert 'c->role=SA_ROLE_SLAVE;c->device_id=1001' not in cm

ns = need('STM32_Firmware/app/node/node_service.c',
          'SA_DEVICE_LOCAL_ESP',
          'ESP_IDENTITY_WARN_MS 3000u',
          'radio stays WAIT_ESP',
          'source=ESP',
          'network-side NODE_SET',
          '0x00020407u')
assert 'send_node_info(SA_DEVICE_PC);\n}' not in ns.split('void NodeService_Init', 1)[1].split('void NodeService_Process', 1)[0], 'NodeService_Init must not announce stale identity'

boot = need('STM32_Firmware/bootloader/bootloader.c',
            'static uint32_t dev_id=0u;static uint16_t group_id=0u;',
            '0x00020407u')

main = need('ESP_Firmware/main/app_main.c',
            'static app_config_t staged_cfg;',
            'at_shell_init(&staged_cfg,&cfg);',
            'cfg.role==ESP_ROLE_UNCONFIGURED',
            'SA_DEVICE_LOCAL_ESP',
            'V2.4.8')

at = need('ESP_Firmware/components/at_shell/at_shell.c',
          'AT+PENDING?', 'AT+CONFIGCHECK', 'AT+BUS?', 'AT+SYNCSTM32',
          '+APPLY:SAVED,RESTARTING', 'app_config_validate(g)')

ac = need('ESP_Firmware/components/app_config/app_config.c',
          'MASTER_DEVICE_MUST_BE_1', 'SLAVE_DEVICE_CANNOT_BE_1',
          'AUTO_DISCOVERY', 'CHANNEL_SCAN', 'NETWORK_ID', 'CHANNEL')

for proto in ('STM32_Firmware/app/protocol/smartagri_protocol.h',
              'ESP_Firmware/components/cluster_common/include/smartagri_protocol.h'):
    need(proto, '#define SA_DEVICE_LOCAL_ESP    0u')

print('CONFIG AUTHORITY REGRESSION PASS')
