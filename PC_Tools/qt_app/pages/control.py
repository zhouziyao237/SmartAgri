from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

@register_page(30)
class ControlPage(BasePage):
    page_id='control'; page_title='控制与测试'
    def build_ui(self):
        l=QVBoxLayout(self);gpio=QGroupBox('GPIO 控制');f=QFormLayout(gpio);self.gdev=self.spin(1,0x7fffffff,1001);self.gpin=self.combo([x for x in self.schema['pins'] if x['id']]);self.gval=QComboBox();self.gval.addItem('ON / HIGH',1);self.gval.addItem('OFF / LOW',0);self.gdur=self.spin(0,3600000,0);f.addRow('Node ID',self.gdev);f.addRow('Pin',self.gpin);f.addRow('Value',self.gval);f.addRow('Pulse ms',self.gdur);b=QPushButton('发送 GPIO');b.clicked.connect(self.send_gpio);f.addRow(b);l.addWidget(gpio)
        test=QGroupBox('无传感器手动上报');f2=QFormLayout(test);self.tdev=self.spin(1,0x7fffffff,1001);self.tslot=self.spin(0,7,0);self.tmeas=self.combo(self.schema['measurements'],lambda x:f"{x['id']} · {x['name']}");self.tvalue=self.dspin(25.5);self.tq=self.spin(0,100,100);f2.addRow('Node ID',self.tdev);f2.addRow('Slot',self.tslot);f2.addRow('Measurement',self.tmeas);f2.addRow('Value',self.tvalue);f2.addRow('Quality',self.tq);bb=QPushButton('节点生成一次 TELEMETRY');bb.clicked.connect(self.inject);f2.addRow(bb);l.addWidget(test);l.addStretch()
    def send_gpio(self):
        x=self.call(self.api.post,'/api/gpio',{'device_id':self.gdev.value(),'pin_id':self.gpin.currentData(),'value':self.gval.currentData(),'duration_ms':self.gdur.value()})
        if x:self.app.msg('GPIO 控制成功' if x.get('ok') else str(x.get('result')),not x.get('ok'))
    def inject(self):
        x=self.call(self.api.post,'/api/test-telemetry',{'device_id':self.tdev.value(),'slot_id':self.tslot.value(),'measurement_id':self.tmeas.currentData(),'value':self.tvalue.value(),'quality':self.tq.value()})
        if x:self.app.msg(f"Node {self.tdev.value()} 已生成 TELEMETRY" if x.get('ok') else str(x.get('result')),not x.get('ok'))
