from PySide6.QtWidgets import (
    QComboBox, QFormLayout, QGroupBox, QHBoxLayout, QHeaderView, QLabel,
    QLineEdit, QPushButton, QTableWidget, QTableWidgetItem, QVBoxLayout
)
from ..base import BasePage, register_page

@register_page(40)
class ProfilesPage(BasePage):
    page_id='profiles'; page_title='本地对象模板'

    def build_ui(self):
        root=QVBoxLayout(self)
        editor=QGroupBox('对象模板编辑器')
        form=QHBoxLayout(editor)
        self.name=QLineEdit(); self.name.setPlaceholderText('例如：温室A-PB13输入')
        self.obj=QComboBox(); self.obj.addItem('Sensor Slot','sensor_slot')
        self.mode=QComboBox(); self.mode.addItem('绑定 Node','node'); self.mode.addItem('通用模板','generic')
        self.target=self.spin(1,0x7fffffff,1001)
        self.desc=QLineEdit(); self.desc.setPlaceholderText('说明（可选）')
        for title,w,stretch in [('模板名称',self.name,2),('对象类型',self.obj,1),('绑定模式',self.mode,1),('目标 Node',self.target,1),('说明',self.desc,3)]:
            col=QVBoxLayout(); col.addWidget(QLabel(title)); col.addWidget(w); form.addLayout(col,stretch)
        save=QPushButton('保存当前对象'); save.setObjectName('Primary'); save.clicked.connect(self.save); form.addWidget(save)
        root.addWidget(editor)

        box=QGroupBox('模板库')
        bl=QVBoxLayout(box)
        self.table=QTableWidget(0,5)
        self.table.setHorizontalHeaderLabels(['名称','对象类型','目标','对象数','说明'])
        self.table.horizontalHeader().setSectionResizeMode(QHeaderView.ResizeMode.Stretch)
        self.table.setSelectionBehavior(QTableWidget.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QTableWidget.SelectionMode.SingleSelection)
        self.table.itemSelectionChanged.connect(self.selection_changed)
        bl.addWidget(self.table)

        bind=QHBoxLayout()
        self.mode_edit=QComboBox(); self.mode_edit.addItem('绑定 Node','node'); self.mode_edit.addItem('通用','generic')
        self.target_edit=self.spin(1,0x7fffffff,1001)
        self.mode_edit.currentIndexChanged.connect(self.sync_edit_mode)
        bind.addWidget(QLabel('所选模板')); bind.addWidget(self.mode_edit); bind.addWidget(QLabel('目标 Node')); bind.addWidget(self.target_edit)
        bind.addStretch()
        for text,cb,obj in [
            ('保存绑定',self.update_target,''),('载入到编辑器',self.load_editor,''),('应用到节点',self.apply,'Primary'),('删除',self.delete,'Danger')]:
            b=QPushButton(text); b.clicked.connect(cb)
            if obj:b.setObjectName(obj)
            bind.addWidget(b)
        bl.addLayout(bind)
        root.addWidget(box,1)

        hint=QLabel('扩展方式：在 qt_app/pages/ 新增页面模块 + @register_page；模板对象类型可继续加入 actuator / rule / gateway / scene。')
        hint.setWordWrap(True); root.addWidget(hint)
        self.mode.currentIndexChanged.connect(self.sync_mode)
        self.profiles={}; self.names=[]; self.sync_mode()

    def on_show(self): self.load()
    def sync_mode(self):
        sensor=self.app.page('sensor')
        if self.mode.currentData()=='node' and sensor:self.target.setValue(sensor.get_node())
        self.target.setEnabled(self.mode.currentData()=='node')
    def sync_edit_mode(self): self.target_edit.setEnabled(self.mode_edit.currentData()=='node')
    def load(self):
        x=self.call(self.api.get,'/api/profiles'); self.profiles=x or {}; self.names=sorted(self.profiles)
        self.table.setRowCount(len(self.names))
        for r,name in enumerate(self.names):
            p=self.profiles[name]; t=p.get('target') or {}; target=f"Node {t.get('device_id')}" if t.get('mode')=='node' and t.get('device_id') else 'Generic'
            vals=[name,p.get('object_type','sensor_slot'),target,len(p.get('slots') or []),p.get('description','')]
            for c,v in enumerate(vals): self.table.setItem(r,c,QTableWidgetItem(str(v)))
        if self.names and self.table.currentRow()<0:self.table.selectRow(0)
    def current(self):
        r=self.table.currentRow()
        if r<0 or r>=len(self.names):return None
        return self.profiles.get(self.names[r])
    def selection_changed(self):
        p=self.current()
        if not p:return
        t=p.get('target') or {}; mode='node' if t.get('mode')=='node' and t.get('device_id') else 'generic'
        i=self.mode_edit.findData(mode); self.mode_edit.setCurrentIndex(max(0,i))
        if t.get('device_id'):self.target_edit.setValue(int(t['device_id']))
        self.sync_edit_mode()
    def save(self):
        sensor=self.app.page('sensor'); name=self.name.text().strip()
        if not name:return self.app.msg('请输入模板名称',True)
        x=self.call(self.api.post,'/api/profiles/save',{
            'name':name,'object_type':self.obj.currentData(),'generic':self.mode.currentData()=='generic',
            'target_device_id':self.target.value(),'description':self.desc.text().strip(),
            'slots':[sensor.slot_payload()]})
        if x:self.load(); self.app.msg('模板已保存')
    def load_editor(self):
        p=self.current(); sensor=self.app.page('sensor')
        if not p:return
        if p.get('slots'):sensor.apply_payload(p['slots'][0])
        dev=(p.get('target') or {}).get('device_id')
        if dev:sensor.set_node(dev)
        self.app.show_page('sensor'); self.app.msg(f"模板 {p['name']} 已载入")
    def update_target(self):
        p=self.current()
        if not p:return
        x=self.call(self.api.post,'/api/profiles/target',{
            'name':p['name'],'generic':self.mode_edit.currentData()=='generic','target_device_id':self.target_edit.value()})
        if x:self.load(); self.app.msg('模板 Node 绑定已更新')
    def apply(self):
        p=self.current()
        if not p:return
        dev=self.target_edit.value() if self.mode_edit.currentData()=='node' else self.app.page('sensor').get_node()
        x=self.call(self.api.post,'/api/profiles/apply',{'name':p['name'],'device_id':dev})
        if x:self.app.msg(f"模板 {p['name']} 已应用到 Node {dev}" if x.get('ok') else x.get('error','应用失败'),not x.get('ok'))
    def delete(self):
        p=self.current()
        if not p:return
        x=self.call(self.api.post,'/api/profiles/delete',{'name':p['name']})
        if x:self.load(); self.app.msg('模板已删除')
