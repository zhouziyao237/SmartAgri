from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

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
