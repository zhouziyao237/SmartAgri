"""Auto-discovered Qt page plug-ins.

Add a new .py module in this directory and decorate a BasePage subclass with
@register_page(order). MainWindow does not need to be edited.
"""
from importlib import import_module
from pkgutil import iter_modules

for _m in iter_modules(__path__):
    if not _m.name.startswith('_'):
        import_module(f'{__name__}.{_m.name}')
