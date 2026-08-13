from PySide6.QtWidgets import QWidget, QComboBox, QDoubleSpinBox, QSpinBox

PAGE_REGISTRY = []

def register_page(order=100):
    def deco(cls):
        cls.page_order = order
        PAGE_REGISTRY.append(cls)
        return cls
    return deco

class BasePage(QWidget):
    page_id = 'base'
    page_title = 'Base'
    page_order = 100

    def __init__(self, app):
        super().__init__()
        self.app = app
        self.api = app.api
        self.schema = app.schema
        self.build_ui()

    def build_ui(self):
        pass

    def refresh(self, status):
        pass

    def on_show(self):
        pass

    def call(self, fn, *args):
        return self.app.call(fn, *args)

    @staticmethod
    def combo(items, label=lambda x: x['name']):
        c = QComboBox()
        for x in items:
            c.addItem(label(x), x['id'])
        return c

    @staticmethod
    def spin(lo=0, hi=2_000_000_000, value=0):
        s = QSpinBox(); s.setRange(lo, hi); s.setValue(value); return s

    @staticmethod
    def dspin(value=0, lo=-1e6, hi=1e6):
        s = QDoubleSpinBox(); s.setRange(lo, hi); s.setDecimals(5); s.setValue(value); return s
