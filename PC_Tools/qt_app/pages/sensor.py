from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

@register_page(20)
class SensorConfigPage(BasePage):
    page_id='sensor'; page_title='传感器对象'
    def build_ui(self):
        outer=QVBoxLayout(self); box=QGroupBox('传感器 Slot 图形化配置'); g=QGridLayout(box)
        self.dev=self.spin(1,0x7fffffff,1001); self.slot=self.spin(0,7,0); self.driver=self.combo(self.schema['drivers'],lambda x:f"{x['id']} · {x['name']}"); self.meas=self.combo(self.schema['measurements'],lambda x:f"{x['id']} · {x['name']}"); self.iface=self.combo(self.schema['interfaces'],lambda x:f"{x['id']} · {x['name']}"); self.pin=self.combo(self.schema['pins'],lambda x:f"{x['id']} · {x['name']}")
        self.bus=self.spin(0,255,0); self.addr=self.spin(0,65535,0); self.reg=self.spin(0,65535,0); self.sample=self.spin(1,3600000,1000); self.report=self.spin(1,3600000,1000); self.scale=self.dspin(1); self.offset=self.dspin(0); self.threshold=self.dspin(0); self.flags=self.spin(0,0x7fffffff,0); self.p0=self.spin(0,0x7fffffff,0); self.p1=self.spin(0,0x7fffffff,0); self.enabled=QCheckBox(); self.enabled.setChecked(True)
        fields=[('Node ID',self.dev),('Slot',self.slot),('Driver',self.driver),('Measurement',self.meas),('Interface',self.iface),('Pin',self.pin),('Bus',self.bus),('Address',self.addr),('Register',self.reg),('Sample ms',self.sample),('Report ms',self.report),('Scale',self.scale),('Offset',self.offset),('Threshold',self.threshold),('Flags',self.flags),('Param0',self.p0),('Param1',self.p1),('Enabled',self.enabled)]
        for i,(name,w) in enumerate(fields): g.addWidget(QLabel(name),i//3,(i%3)*2);g.addWidget(w,i//3,(i%3)*2+1)
        self.driver.currentIndexChanged.connect(self.driver_changed); outer.addWidget(box)
        row=QHBoxLayout()
        for text,cb in [('读取 Capability',self.read_caps),('读取当前配置',self.read_config),('应用 Slot',self.apply_slot),('删除 Slot',self.delete_slot)]: b=QPushButton(text);b.clicked.connect(cb);row.addWidget(b)
        row.addStretch();outer.addLayout(row);self.caps=QLabel('Capability: 尚未读取');self.caps.setWordWrap(True);outer.addWidget(self.caps)
        self.config=QTableWidget(0,8);self.config.setHorizontalHeaderLabels(['Slot','Enabled','Driver','Measurement','Interface','Pin','Sample','操作']);self.config.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);outer.addWidget(self.config)
        i=self.driver.findData(1); self.driver.setCurrentIndex(max(0,i)); self.driver_changed()
    def get_node(self): return self.dev.value()
    def set_node(self,d): self.dev.setValue(int(d))
    def driver_changed(self):
        d=next((x for x in self.schema['drivers'] if x['id']==self.driver.currentData()),None)
        if not d:return
        z=d.get('defaults') or {}
        for k,c in [('interface_type',self.iface),('measurement_id',self.meas),('pin_id',self.pin)]:
            if k in z:
                i=c.findData(z[k]);
                if i>=0:c.setCurrentIndex(i)
        if 'address' in z:self.addr.setValue(z['address'])
        if 'sample_interval_ms' in z:self.sample.setValue(z['sample_interval_ms'])
        if 'report_interval_ms' in z:self.report.setValue(z['report_interval_ms'])
    def slot_payload(self):
        return {'slot_id':self.slot.value(),'enabled':self.enabled.isChecked(),'driver_id':self.driver.currentData(),'measurement_id':self.meas.currentData(),'interface_type':self.iface.currentData(),'bus_id':self.bus.value(),'pin_id':self.pin.currentData(),'address':self.addr.value(),'reg':self.reg.value(),'sample_interval_ms':self.sample.value(),'report_interval_ms':self.report.value(),'scale':self.scale.value(),'offset':self.offset.value(),'threshold':self.threshold.value(),'flags':self.flags.value(),'param0':self.p0.value(),'param1':self.p1.value()}
    def apply_payload(self,s):
        self.slot.setValue(int(s.get('slot_id',0)));self.enabled.setChecked(bool(s.get('enabled',True)))
        for c,k in [(self.driver,'driver_id'),(self.meas,'measurement_id'),(self.iface,'interface_type'),(self.pin,'pin_id')]:
            i=c.findData(s.get(k));
            if i>=0:c.setCurrentIndex(i)
        for w,k in [(self.bus,'bus_id'),(self.addr,'address'),(self.reg,'reg'),(self.sample,'sample_interval_ms'),(self.report,'report_interval_ms'),(self.flags,'flags'),(self.p0,'param0'),(self.p1,'param1')]:w.setValue(int(s.get(k,0)))
        for w,k in [(self.scale,'scale'),(self.offset,'offset'),(self.threshold,'threshold')]:w.setValue(float(s.get(k,0)))
    def read_caps(self):
        x=self.call(self.api.get,f'/api/caps/{self.get_node()}')
        if x and x.get('ok'):
            c=x['caps'];self.caps.setText('Capability: '+', '.join(f"{p['pin_name']}[{'/'.join(p['cap_names'])}]" for p in c.get('pins',[])))
    def read_config(self):
        x=self.call(self.api.get,f'/api/config/{self.get_node()}')
        if not x or not x.get('ok'):return
        slots=x['config'].get('slots',[]);self.config.setRowCount(len(slots))
        for r,s in enumerate(slots):
            vals=[s['slot_id'],s['enabled'],s['driver_name'],s['measurement_name'],s['interface_name'],s['pin_name'],s['sample_interval_ms']]
            for c,v in enumerate(vals):self.config.setItem(r,c,QTableWidgetItem(str(v)))
            b=QPushButton('编辑');b.clicked.connect(lambda _=False,ss=s:self.apply_payload(ss));self.config.setCellWidget(r,7,b)
    def apply_slot(self):
        x=self.call(self.api.post,'/api/configure',{'device_id':self.get_node(),'slots':[self.slot_payload()]})
        if x:self.app.msg(f"Node {self.get_node()} 配置成功并 Commit" if x.get('ok') else x.get('error','配置失败'),not x.get('ok'))
    def delete_slot(self):
        x=self.call(self.api.post,'/api/delete-slot',{'device_id':self.get_node(),'slot_id':self.slot.value()})
        if x:self.app.msg('Slot 已删除' if x.get('ok') else x.get('error','删除失败'),not x.get('ok'))
