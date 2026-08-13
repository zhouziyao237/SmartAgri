from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from .base import BasePage, register_page

@register_page(10)
class DashboardPage(BasePage):
    page_id='overview'; page_title='设备中心'
    def build_ui(self):
        l=QVBoxLayout(self)
        self.table=QTableWidget(0,8); self.table.setHorizontalHeaderLabels(['Device','Role','状态','Last seen','Network','Group','Generation','最新遥测']); self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch); l.addWidget(self.table)
    def refresh(self,status):
        nodes=list((status.get('nodes') or {}).values()); self.table.setRowCount(len(nodes))
        for r,n in enumerate(nodes):
            latest=[]
            for t in (n.get('telemetry') or {}).values():
                for v in t.get('values',[]): latest.append(f"{v.get('measurement_name',v.get('measurement_id'))}={v.get('value',0):.3f}")
            vals=[n.get('device_id'),n.get('role_name',n.get('role','?')),'ONLINE' if n.get('online') else 'OFFLINE',n.get('age_sec','--'),n.get('network_id','?'),n.get('group_id','?'),n.get('generation',0),'; '.join(latest)]
            for c,v in enumerate(vals): self.table.setItem(r,c,QTableWidgetItem(str(v)))

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

@register_page(40)
class ProfilesPage(BasePage):
    page_id='profiles'; page_title='本地对象模板'
    def build_ui(self):
        l=QHBoxLayout(self);left=QGroupBox('保存当前编辑器对象');f=QFormLayout(left);self.name=QLineEdit();self.obj=QComboBox();self.obj.addItem('Sensor Slot','sensor_slot');self.mode=QComboBox();self.mode.addItem('绑定 Node','node');self.mode.addItem('通用模板','generic');self.target=self.spin(1,0x7fffffff,1001);self.desc=QLineEdit();f.addRow('模板名称',self.name);f.addRow('对象类型',self.obj);f.addRow('绑定模式',self.mode);f.addRow('目标 Node',self.target);f.addRow('说明',self.desc);b=QPushButton('保存模板');b.clicked.connect(self.save);f.addRow(b);l.addWidget(left)
        right=QGroupBox('模板列表（每个模板独立绑定 Node）');rl=QVBoxLayout(right);self.list=QListWidget();rl.addWidget(self.list);self.target_edit=self.spin(1,0x7fffffff,1001);self.mode_edit=QComboBox();self.mode_edit.addItem('绑定 Node','node');self.mode_edit.addItem('通用','generic');fr=QHBoxLayout();fr.addWidget(QLabel('当前模板模式'));fr.addWidget(self.mode_edit);fr.addWidget(QLabel('目标 Node'));fr.addWidget(self.target_edit);rl.addLayout(fr);row=QHBoxLayout();
        for text,cb in [('载入编辑器',self.load_editor),('更新绑定',self.update_target),('应用到节点',self.apply),('删除',self.delete)]:bb=QPushButton(text);bb.clicked.connect(cb);row.addWidget(bb)
        rl.addLayout(row);l.addWidget(right);self.mode.currentIndexChanged.connect(self.sync_mode);self.mode_edit.currentIndexChanged.connect(self.sync_edit_mode);self.list.currentItemChanged.connect(self.selection_changed);self.profiles={};self.sync_mode()
    def on_show(self): self.load()
    def sync_mode(self):
        sensor=self.app.page('sensor');
        if self.mode.currentData()=='node' and sensor:self.target.setValue(sensor.get_node())
        self.target.setEnabled(self.mode.currentData()=='node')
    def sync_edit_mode(self): self.target_edit.setEnabled(self.mode_edit.currentData()=='node')
    def load(self):
        x=self.call(self.api.get,'/api/profiles');self.profiles=x or {};self.list.clear()
        for name,p in sorted(self.profiles.items()):
            t=p.get('target') or {};target=f"Node {t.get('device_id')}" if t.get('mode')=='node' and t.get('device_id') else '通用';item=QListWidgetItem(f"{name}   [{p.get('object_type','sensor_slot')}]   {target}   {len(p.get('slots') or [])} slot(s)");item.setData(256,name);self.list.addItem(item)
    def current(self):
        item=self.list.currentItem();return self.profiles.get(item.data(256)) if item else None
    def selection_changed(self,*_):
        p=self.current();
        if not p:return
        t=p.get('target') or {};mode='node' if t.get('mode')=='node' and t.get('device_id') else 'generic';i=self.mode_edit.findData(mode);self.mode_edit.setCurrentIndex(max(0,i));
        if t.get('device_id'):self.target_edit.setValue(int(t['device_id']))
        self.sync_edit_mode()
    def save(self):
        sensor=self.app.page('sensor');name=self.name.text().strip()
        if not name:return self.app.msg('请输入模板名称',True)
        x=self.call(self.api.post,'/api/profiles/save',{'name':name,'object_type':self.obj.currentData(),'generic':self.mode.currentData()=='generic','target_device_id':self.target.value(),'description':self.desc.text().strip(),'slots':[sensor.slot_payload()]})
        if x:self.load();self.app.msg('模板已保存')
    def load_editor(self):
        p=self.current();sensor=self.app.page('sensor')
        if not p:return
        if p.get('slots'):sensor.apply_payload(p['slots'][0])
        dev=(p.get('target') or {}).get('device_id')
        if dev:sensor.set_node(dev)
        self.app.show_page('sensor');self.app.msg(f"模板 {p['name']} 已载入")
    def update_target(self):
        p=self.current();
        if not p:return
        x=self.call(self.api.post,'/api/profiles/target',{'name':p['name'],'generic':self.mode_edit.currentData()=='generic','target_device_id':self.target_edit.value()})
        if x:self.load();self.app.msg('模板 Node 绑定已更新')
    def apply(self):
        p=self.current();
        if not p:return
        dev=self.target_edit.value() if self.mode_edit.currentData()=='node' else self.app.page('sensor').get_node()
        x=self.call(self.api.post,'/api/profiles/apply',{'name':p['name'],'device_id':dev})
        if x:self.app.msg(f"模板 {p['name']} 已应用到 Node {dev}" if x.get('ok') else x.get('error','应用失败'),not x.get('ok'))
    def delete(self):
        p=self.current();
        if not p:return
        x=self.call(self.api.post,'/api/profiles/delete',{'name':p['name']})
        if x:self.load();self.app.msg('模板已删除')

@register_page(50)
class OtaPage(BasePage):
    page_id='ota'; page_title='远程 OTA'
    def build_ui(self):
        outer=QVBoxLayout(self)
        box=QGroupBox('Node 固件组件'); f=QFormLayout(box)
        self.dev=self.spin(1,0x7fffffff,1001); self.component=QComboBox(); self.component.addItem('STM32 MCU','stm32'); self.component.addItem('ESP32 / ESP32-C3','esp')
        self.file=QLineEdit(); self.file.setReadOnly(True)
        pick=QPushButton('选择 .bin'); pick.clicked.connect(self.choose_file); fr=QHBoxLayout(); fr.addWidget(self.file,1); fr.addWidget(pick)
        f.addRow('Node ID',self.dev); f.addRow('组件',self.component); f.addRow('Firmware',fr)
        row=QHBoxLayout(); probe=QPushButton('读取目标信息'); probe.clicked.connect(self.probe); start=QPushButton('开始 OTA'); start.setObjectName('Primary'); start.clicked.connect(self.start); row.addWidget(probe); row.addWidget(start); row.addStretch(); f.addRow(row)
        outer.addWidget(box)
        self.info=QLabel('目标信息：尚未读取'); self.info.setWordWrap(True); outer.addWidget(self.info)
        jobs=QGroupBox('OTA 任务'); jl=QVBoxLayout(jobs); self.table=QTableWidget(0,7); self.table.setHorizontalHeaderLabels(['Job','Node','组件','文件','状态','进度','信息']); self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch); jl.addWidget(self.table); outer.addWidget(jobs,1)
        self.path=''
    def choose_file(self):
        p,_=QFileDialog.getOpenFileName(self,'选择固件','','Firmware (*.bin);;All files (*.*)')
        if p:self.path=p;self.file.setText(p)
    def probe(self):
        x=self.call(self.api.get,f"/api/ota/info/{self.dev.value()}/{self.component.currentData()}")
        if x and x.get('ok'):
            i=x['info']; self.info.setText(f"目标：{i.get('component_name')} / {i.get('target_name')}   FW=0x{int(i.get('fw_version',0)):X}   Max={int(i.get('max_image_size',0))/1024:.1f} KB   State={i.get('state')}")
    def start(self):
        if not self.path:return self.app.msg('请先选择 .bin 固件',True)
        try:data=open(self.path,'rb').read()
        except Exception as e:return self.app.msg(str(e),True)
        from urllib.parse import urlencode
        q=urlencode({'device_id':self.dev.value(),'component':self.component.currentData(),'filename':self.path.replace('\\','/').split('/')[-1]})
        x=self.call(self.api.post_binary,f'/api/ota/start?{q}',data,30)
        if x:self.app.msg(f"OTA 任务 {x.get('job',{}).get('job_id','')} 已启动")
    def refresh(self,status):
        jobs=sorted((status.get('ota_jobs') or {}).values(),key=lambda x:x.get('created_at',0),reverse=True); self.table.setRowCount(len(jobs))
        for r,j in enumerate(jobs):
            vals=[j.get('job_id'),j.get('device_id'),j.get('component_name'),j.get('filename'),j.get('state'),f"{j.get('progress',0)}%",j.get('message',j.get('error',''))]
            for c,v in enumerate(vals):self.table.setItem(r,c,QTableWidgetItem(str(v)))

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

@register_page(70)
class EventsPage(BasePage):
    page_id='events'; page_title='事件日志'
    def build_ui(self):
        l=QVBoxLayout(self);self.table=QTableWidget(0,5);self.table.setHorizontalHeaderLabels(['时间','来源','CMD','名称','长度']);self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch);l.addWidget(self.table)
    def refresh(self,status):
        ev=(status.get('events') or [])[-100:][::-1];self.table.setRowCount(len(ev))
        for r,e in enumerate(ev):
            x=e.get('event',{});vals=[e.get('time',''),x.get('src','') if isinstance(x,dict) else '',x.get('cmd','') if isinstance(x,dict) else '',x.get('name',x if isinstance(x,str) else '') if isinstance(x,dict) else x,x.get('len','') if isinstance(x,dict) else '']
            for c,v in enumerate(vals):self.table.setItem(r,c,QTableWidgetItem(str(v)))
