import json
import urllib.error
import webbrowser
from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QAction
from PySide6.QtWidgets import (
    QFrame, QHBoxLayout, QLabel, QListWidget, QListWidgetItem, QMainWindow,
    QMessageBox, QPushButton, QStackedWidget, QStatusBar, QVBoxLayout, QWidget
)
from .base import PAGE_REGISTRY
from . import pages  # registers page classes

APP_QSS = r'''
QMainWindow,QWidget{background:#f4f6f5;color:#17211c;font-family:"Microsoft YaHei";font-size:13px}
QFrame#TopBar{background:#0c3b2b;border:0} QLabel#Brand{color:white;font-size:18px;font-weight:700} QLabel#Conn{color:#d6eee2;font-weight:600}
QListWidget#Nav{background:white;border:0;border-right:1px solid #dfe6e2;padding:12px 8px;outline:0}
QListWidget#Nav::item{height:42px;border-radius:8px;padding-left:12px;color:#35423b;font-weight:600}
QListWidget#Nav::item:selected{background:#e8f5ee;color:#0b5e3e} QListWidget#Nav::item:hover{background:#f3f7f5}
QGroupBox{background:white;border:1px solid #dfe6e2;border-radius:10px;margin-top:12px;padding:14px;font-weight:700}
QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px} QLineEdit,QComboBox,QSpinBox,QDoubleSpinBox{background:white;border:1px solid #ccd6d0;border-radius:7px;padding:6px;min-height:24px}
QPushButton{background:white;border:1px solid #cbd5cf;border-radius:7px;padding:7px 11px;font-weight:600} QPushButton:hover{background:#f7f9f8}
QPushButton#Primary{background:#0e7a50;color:white;border-color:#0e7a50} QPushButton#Danger{color:#b42318;border-color:#efb7b2}
QTableWidget{background:white;border:1px solid #dfe6e2;border-radius:8px;gridline-color:#edf1ef} QHeaderView::section{background:#f8faf9;border:0;border-bottom:1px solid #dfe6e2;padding:7px;color:#59675f;font-weight:700}
QProgressBar{border:1px solid #dfe6e2;border-radius:6px;background:#edf1ef;text-align:center} QProgressBar::chunk{background:#0e7a50;border-radius:5px}
QStatusBar{background:white;border-top:1px solid #dfe6e2}
'''

class MainWindow(QMainWindow):
    def __init__(self, api, backend_proc=None):
        super().__init__(); self.api=api; self.backend_proc=backend_proc; self.schema=self.api.get('/api/schema'); self.pages={}
        self.setWindowTitle('SmartAgri V2.4.8 - Unified Qt Control Center'); self.resize(1380,900); self.setStyleSheet(APP_QSS)
        self.build(); self.timer=QTimer(self); self.timer.timeout.connect(self.refresh); self.timer.start(1200); self.refresh()

    def build(self):
        self.setStatusBar(QStatusBar())
        menu=self.menuBar().addMenu('工具'); a=QAction('打开 Web 上位机',self); a.triggered.connect(lambda:webbrowser.open('http://127.0.0.1:8080')); menu.addAction(a); a2=QAction('发现全部节点',self); a2.triggered.connect(self.discover); menu.addAction(a2)
        root=QWidget(); root_l=QVBoxLayout(root); root_l.setContentsMargins(0,0,0,0); root_l.setSpacing(0)
        top=QFrame(); top.setObjectName('TopBar'); top.setFixedHeight(64); tl=QHBoxLayout(top); tl.setContentsMargins(20,0,20,0)
        brand=QLabel('SmartAgri   V2.4.8'); brand.setObjectName('Brand'); self.conn=QLabel('MASTER: scanning...'); self.conn.setObjectName('Conn')
        discover=QPushButton('发现全部节点'); discover.clicked.connect(self.discover); discover.setStyleSheet('color:white;border-color:#ffffff55;background:#ffffff12')
        web=QPushButton('打开 Web'); web.clicked.connect(lambda:webbrowser.open('http://127.0.0.1:8080')); web.setStyleSheet('color:white;border-color:#ffffff55;background:#ffffff12')
        tl.addWidget(brand); tl.addStretch(); tl.addWidget(self.conn); tl.addSpacing(12); tl.addWidget(discover); tl.addWidget(web); root_l.addWidget(top)
        body=QWidget(); bl=QHBoxLayout(body); bl.setContentsMargins(0,0,0,0); bl.setSpacing(0)
        self.nav=QListWidget(); self.nav.setObjectName('Nav'); self.nav.setFixedWidth(210); self.stack=QStackedWidget(); self.stack.setContentsMargins(18,18,18,18)
        bl.addWidget(self.nav); bl.addWidget(self.stack,1); root_l.addWidget(body,1); self.setCentralWidget(root)
        for cls in sorted(PAGE_REGISTRY,key=lambda x:x.page_order):
            page=cls(self); self.pages[page.page_id]=page; self.stack.addWidget(page); item=QListWidgetItem(page.page_title); item.setData(Qt.ItemDataRole.UserRole,page.page_id); self.nav.addItem(item)
        self.nav.currentRowChanged.connect(self.page_changed); self.nav.setCurrentRow(0)

    def page(self,page_id): return self.pages.get(page_id)
    def show_page(self,page_id):
        for i in range(self.nav.count()):
            if self.nav.item(i).data(Qt.ItemDataRole.UserRole)==page_id: self.nav.setCurrentRow(i); return
    def page_changed(self,index):
        if index<0:return
        self.stack.setCurrentIndex(index); w=self.stack.widget(index)
        if hasattr(w,'on_show'):w.on_show()
    def closeEvent(self,event):
        if self.backend_proc and self.backend_proc.poll() is None:self.backend_proc.terminate()
        super().closeEvent(event)
    def msg(self,text,error=False):
        self.statusBar().showMessage(str(text),6000)
        if error:QMessageBox.warning(self,'SmartAgri',str(text))
    def call(self,fn,*args):
        try:return fn(*args)
        except urllib.error.HTTPError as e:
            try:detail=json.loads(e.read().decode()).get('error',str(e))
            except Exception:detail=str(e)
            self.msg(detail,True)
        except Exception as e:self.msg(str(e),True)
        return None
    def discover(self):
        x=self.call(self.api.post,'/api/discover',{})
        if x:self.msg('已广播发现命令')
    def refresh(self):
        s=self.call(self.api.get,'/api/status')
        if not s:return
        self.conn.setText(f"MASTER {s['master']} @ {s['baud']}" if s['connected'] else 'MASTER 自动扫描中')
        for p in self.pages.values():p.refresh(s)
