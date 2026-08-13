import json
from PySide6.QtWidgets import (
    QComboBox, QFileDialog, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QVBoxLayout
)
from ..base import BasePage, register_page


@register_page(25)
class RadioConfigPage(BasePage):
    page_id = 'radio'
    page_title = '无线与节点部署'

    def build_ui(self):
        outer = QVBoxLayout(self)
        box = QGroupBox('ESP Radio Config / Zero-touch Deployment')
        g = QGridLayout(box)

        self.target = self.spin(1, 0x7fffffff, 1001)
        self.role = QComboBox()
        self.role.addItem('UNCONFIGURED', 0)
        self.role.addItem('MASTER', 1)
        self.role.addItem('SLAVE', 2)
        self.new_device = self.spin(1, 0x7fffffff, 1001)
        self.network = self.spin(1, 65535, 1)
        self.group = self.spin(0, 65535, 1)
        self.channel = self.spin(1, 13, 6)
        self.power = QComboBox(); self.power.addItem('ACTIVE', 0); self.power.addItem('BALANCED', 1); self.power.addItem('LOW', 2)
        self.uplink = QComboBox(); self.uplink.addItem('NONE', 0); self.uplink.addItem('TCP', 1)
        self.auto = QComboBox(); self.auto.addItem('Enabled', 1); self.auto.addItem('Disabled', 0)
        self.scan = QComboBox(); self.scan.addItem('Enabled', 1); self.scan.addItem('Disabled', 0)
        self.gateway = QLineEdit(); self.gateway.setPlaceholderText('留空 = 未绑定 / 自动广播发现')

        fields = [
            ('当前/临时 Node ID', self.target), ('Role', self.role), ('新 Device ID', self.new_device),
            ('Network ID', self.network), ('Group ID', self.group), ('Preferred Channel', self.channel),
            ('Auto Discovery', self.auto), ('Channel Scan', self.scan), ('Power Mode', self.power),
            ('Uplink', self.uplink), ('Gateway MAC', self.gateway),
        ]
        for i, (name, w) in enumerate(fields):
            g.addWidget(QLabel(name), i // 2, (i % 2) * 2)
            g.addWidget(w, i // 2, (i % 2) * 2 + 1)
        outer.addWidget(box)

        note = QLabel('V2.4.8 新 SLAVE 可不填 Gateway：会在 1–13 Channel 发送真实 ESP-NOW 广播 LINK_HELLO，收到 MASTER ACK 后自动绑定并保存。旧 Protocol-2 节点保持基础兼容，但可能不支持远程 Radio Config。')
        note.setWordWrap(True); outer.addWidget(note)

        row = QHBoxLayout()
        for text, cb in [
            ('读取 Radio Config', self.read_radio), ('应用并重启 ESP', self.apply_radio),
            ('下载完整配置 JSON', self.export_json), ('导入完整配置 JSON', self.import_json),
        ]:
            b = QPushButton(text); b.clicked.connect(cb); row.addWidget(b)
        row.addStretch(); outer.addLayout(row)
        self.info = QLabel('尚未读取 Radio Config'); self.info.setWordWrap(True); outer.addWidget(self.info)
        outer.addStretch()

    def _set_combo(self, c, value):
        i = c.findData(int(value))
        if i >= 0: c.setCurrentIndex(i)

    def fill(self, c):
        if not c: return
        self._set_combo(self.role, c.get('role', 2))
        self.new_device.setValue(int(c.get('device_id', self.target.value())))
        self.network.setValue(int(c.get('network_id', 1)))
        self.group.setValue(int(c.get('group_id', 1)))
        self.channel.setValue(int(c.get('channel', 6)))
        self._set_combo(self.power, c.get('power_mode', 1))
        self._set_combo(self.uplink, c.get('uplink', 0))
        self._set_combo(self.auto, 1 if c.get('auto_discovery', True) else 0)
        self._set_combo(self.scan, 1 if c.get('channel_scan', True) else 0)
        self.gateway.setText(c.get('gateway_mac', '') if c.get('gateway_valid') else '')
        caps = ', '.join(c.get('capability_names') or []) or 'Legacy / unknown'
        self.info.setText(f"FW {c.get('fw_version_text','?')} | Protocol {c.get('proto_min','?')}–{c.get('proto_max','?')} | Caps: {caps} | Gateway: {c.get('gateway_mac') if c.get('gateway_valid') else 'UNBOUND'}")

    def payload(self):
        gw = self.gateway.text().strip()
        return {
            'role': int(self.role.currentData()), 'device_id': self.new_device.value(),
            'network_id': self.network.value(), 'group_id': self.group.value(),
            'channel': self.channel.value(), 'power_mode': int(self.power.currentData()),
            'uplink': int(self.uplink.currentData()), 'auto_discovery': bool(self.auto.currentData()),
            'channel_scan': bool(self.scan.currentData()), 'gateway_valid': bool(gw), 'gateway_mac': gw,
        }

    def read_radio(self):
        x = self.call(self.api.get, f'/api/radio/{self.target.value()}')
        if x and x.get('ok'):
            self.fill(x.get('radio')); self.app.msg(f"Node {self.target.value()} Radio Config 已读取")

    def apply_radio(self):
        dev = self.target.value()
        x = self.call(self.api.post, '/api/radio/configure', {
            'device_id': dev, 'group_id': self.group.value(), 'radio': self.payload()
        })
        if x:
            self.app.msg('Radio Config 已保存，ESP 将重启并重新发现' if x.get('ok') else str(x.get('result', x)), not x.get('ok'))

    def export_json(self):
        dev = self.target.value()
        x = self.call(self.api.get, f'/api/export/{dev}')
        if not x or not x.get('ok'): return
        path, _ = QFileDialog.getSaveFileName(self, '保存 SmartAgri 节点配置', f'SmartAgri_Node_{dev}_config.json', 'JSON (*.json)')
        if not path: return
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(x.get('bundle'), f, ensure_ascii=False, indent=2)
        self.app.msg(f'配置已保存：{path}')

    def import_json(self):
        path, _ = QFileDialog.getOpenFileName(self, '导入 SmartAgri 节点配置', '', 'JSON (*.json)')
        if not path: return
        try:
            with open(path, 'r', encoding='utf-8') as f: bundle = json.load(f)
        except Exception as e:
            self.app.msg(f'JSON 读取失败：{e}', True); return
        x = self.call(self.api.post, '/api/config/import', {
            'target_device_id': self.target.value(), 'group_id': self.group.value(), 'bundle': bundle
        })
        if x:
            self.app.msg('完整配置已导入；Radio 变化会在 ESP 重启后生效' if x.get('ok') else json.dumps(x, ensure_ascii=False), not x.get('ok'))
