"""Optional read-only hardware integration test; uses its own program directory."""
from pathlib import Path
import configparser, shutil, subprocess, tempfile, time, sys
import ctypes as C
from ctypes import wintypes as W
source=Path(sys.argv[1]).resolve()
with tempfile.TemporaryDirectory(prefix='tm-temperature-') as folder:
    program=Path(folder)
    shutil.copy2(source,program/'SimpleTaskbarMonitor.exe')
    shutil.copytree(source.parent/'sensors',program/'sensors')
    (program/'config.ini').write_text('[general]\nhardware_monitor_item=3\n[task_bar]\ntbar_display_item=96\n[other]\nno_multistart_warning=true\n')
    p=subprocess.Popen([str(program/'SimpleTaskbarMonitor.exe')],cwd=program)
    try:
        cache=program/'temperature.cache.ini'
        deadline=time.monotonic()+25
        while time.monotonic()<deadline and p.poll() is None and not cache.exists():time.sleep(.2)
        assert p.poll() is None, 'Host exited'
        assert cache.exists(), 'Lite host did not start its sensor worker'
        data=configparser.ConfigParser();data.read(cache,encoding='utf-16')
        assert int(data['temperature']['updated'])>0
        print('PASS: Lite host sensor integration:',dict(data['temperature']),flush=True)
        u=C.WinDLL('user32');u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
        cbtype=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
        u.EnumWindows.argtypes=[cbtype,W.LPARAM];u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
        @cbtype
        def close(h,_):
            pid=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(pid))
            name=C.create_unicode_buffer(128);u.GetClassNameW(h,name,128)
            if pid.value==p.pid and name.value=='SimpleTaskbarMonitor_r7XZaS4p':u.PostMessageW(h,0x10,0,0)
            return True
        u.EnumWindows(close,0)
        assert p.wait(timeout=10)==0, 'Host did not exit cleanly'
    finally:
        if p.poll() is None:p.terminate();p.wait(timeout=5)
