from PySide6.QtWidgets import (
    QCheckBox, QComboBox, QFormLayout, QGridLayout, QGroupBox, QHBoxLayout, QHeaderView,
    QLabel, QLineEdit, QListWidget, QListWidgetItem, QPushButton, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget, QFileDialog, QProgressBar
)
from ..base import BasePage, register_page

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
