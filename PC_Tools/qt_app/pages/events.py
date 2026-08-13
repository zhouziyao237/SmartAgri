from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

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
