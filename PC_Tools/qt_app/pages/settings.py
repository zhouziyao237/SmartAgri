from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

@register_page(60)
class SettingsPage(BasePage):
    page_id='settings'; page_title='本地设置'
    def build_ui(self):
        f=QFormLayout(self);self.port=QLineEdit();self.baud=self.spin(1200,2000000,115200);self.off=self.dspin(15,1,3600);self.hide=self.dspin(60,1,86400);self.scan=self.dspin(2,.2,60);self.show=QCheckBox();f.addRow('Preferred COM',self.port);f.addRow('Baud',self.baud);f.addRow('Offline timeout (s)',self.off);f.addRow('Hide timeout (s)',self.hide);f.addRow('Scan interval (s)',self.scan);f.addRow('显示离线节点',self.show);b=QPushButton('保存本地设置');b.clicked.connect(self.save);f.addRow(b)
    def on_show(self): self.load()
    def load(self):
        s=self.call(self.api.get,'/api/settings');
        if not s:return
        self.port.setText(s.get('preferred_port',''));self.baud.setValue(int(s.get('baud',115200)));self.off.setValue(float(s.get('node_offline_timeout_sec',15)));self.hide.setValue(float(s.get('node_hide_timeout_sec',60)));self.scan.setValue(float(s.get('scan_interval_sec',2)));self.show.setChecked(bool(s.get('show_offline_nodes',False)))
    def save(self):
        x=self.call(self.api.post,'/api/settings',{'preferred_port':self.port.text().strip(),'baud':self.baud.value(),'node_offline_timeout_sec':self.off.value(),'node_hide_timeout_sec':self.hide.value(),'scan_interval_sec':self.scan.value(),'show_offline_nodes':self.show.isChecked()})
        if x:self.app.msg('本地设置已保存；COM/Baud 在下次重连生效')
