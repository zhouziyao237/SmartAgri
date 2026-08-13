import argparse, os, pathlib, platform, struct, sys, traceback

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--quick',action='store_true'); a=ap.parse_args()
    print('Python:',sys.version.replace('\n',' ')); print('Executable:',sys.executable)
    print('Architecture:',struct.calcsize('P')*8,'bit'); print('Platform:',platform.platform())
    print('PYTHONNOUSERSITE:',os.environ.get('PYTHONNOUSERSITE',''))
    try:
        import serial; print('pyserial:',serial.__version__)
        import PySide6
        from PySide6 import QtCore,QtGui,QtWidgets
        root=pathlib.Path(PySide6.__file__).resolve().parent
        plugins=root/'plugins'; platforms=plugins/'platforms'; qwindows=platforms/'qwindows.dll'
        if plugins.exists(): os.environ.setdefault('QT_PLUGIN_PATH',str(plugins))
        if platforms.exists(): os.environ.setdefault('QT_QPA_PLATFORM_PLUGIN_PATH',str(platforms))
        print('PySide6 root:',root); print('PySide6 Qt:',QtCore.qVersion())
        print('Qt plugins:',plugins,'exists=',plugins.exists())
        if sys.platform.startswith('win'): print('qwindows.dll:',qwindows,'exists=',qwindows.exists())
        print('QT_PLUGIN_PATH:',os.environ.get('QT_PLUGIN_PATH','')); print('QT_QPA_PLATFORM_PLUGIN_PATH:',os.environ.get('QT_QPA_PLATFORM_PLUGIN_PATH',''))
        if not a.quick:
            app=QtWidgets.QApplication.instance() or QtWidgets.QApplication([])
            w=QtWidgets.QWidget(); w.setWindowTitle('SmartAgri Qt diagnostic'); print('QApplication: OK')
            w.deleteLater(); app.quit()
        return 0
    except Exception:
        traceback.print_exc(); return 1
if __name__=='__main__': raise SystemExit(main())
