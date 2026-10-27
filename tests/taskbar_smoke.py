import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import argparse, re, subprocess, tempfile, time

u=C.WinDLL('user32',use_last_error=True)
u.GetDesktopWindow.restype=W.HWND
u.FindWindowW.argtypes=[W.LPCWSTR,W.LPCWSTR]; u.FindWindowW.restype=W.HWND
u.GetAncestor.argtypes=[W.HWND,W.UINT]; u.GetAncestor.restype=W.HWND
u.SendMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]; u.SendMessageW.restype=C.c_ssize_t
u.GetParent.argtypes=[W.HWND]; u.GetParent.restype=W.HWND
u.IsWindowVisible.argtypes=[W.HWND]
u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
u.GetClassNameW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
cbtype=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.EnumChildWindows.argtypes=[W.HWND,cbtype,W.LPARAM]

def windows(pid):
    result=[]
    @cbtype
    def cb(h,_):
        owner=W.DWORD(); u.GetWindowThreadProcessId(h,C.byref(owner))
        if owner.value==pid:
            cls=C.create_unicode_buffer(256); title=C.create_unicode_buffer(512)
            u.GetClassNameW(h,cls,256); u.GetWindowTextW(h,title,512)
            result.append((h,cls.value,title.value,bool(u.IsWindowVisible(h)),u.GetParent(h)))
        return True
    u.EnumChildWindows(u.GetDesktopWindow(),cb,0)
    return result

parser=argparse.ArgumentParser(description='Windows smoke test for a Debug (lite) build; close other TrafficMonitor instances first.')
parser.add_argument('executable',type=Path)
args=parser.parse_args()
exe=args.executable.resolve()
assert exe.is_file(), exe
assert exe.parent.name.lower().startswith('debug'), 'Use a Debug build so configuration stays in the test working directory'
assert not u.FindWindowW('TrafficMonitor_r7XZaS4p',None), 'Close existing TrafficMonitor instances first'
resource_header=Path(__file__).resolve().parents[1]/'TrafficMonitor/resource.h'
header=resource_header.read_text(encoding='utf-8-sig')
option_id=int(re.search(r'#define ID_OPTIONS2\s+(\d+)',header)[1])
temporary=tempfile.TemporaryDirectory(prefix='trafficmonitor-smoke-')
work=Path(temporary.name)
for mode in ['core','display']:
    legacy='show_task_bar_wnd=false\n' if mode=='display' else ''
    (work/'config.ini').write_text('[config]\n'+legacy+'show_notify_icon=true\n[other]\nno_multistart_warning=true\n',encoding='utf-8')
    p=subprocess.Popen([str(exe)],cwd=work)
    try:
        time.sleep(5)
        found=windows(p.pid)
        print(mode,'startup:', 'alive' if p.poll() is None else p.poll(),flush=True)
        assert p.poll() is None, 'Application exited during startup'
        core=next(x for x in found if x[1]=='TrafficMonitor_r7XZaS4p')
        assert not core[3], 'Core window must remain hidden'
        display=next(x for x in found if x[2]=='TrafficMonitorTaskbarWindow' and x[3])
        taskbar=u.GetAncestor(display[0],1)
        cls=C.create_unicode_buffer(256); u.GetClassNameW(taskbar,cls,256)
        assert cls.value in ('Shell_TrayWnd','Shell_SecondaryTrayWnd'), ('Display did not embed in the taskbar',found)
        # Exercise the same options command used by the taskbar menu.
        if mode=='core':
            u.PostMessageW(core[0],0x111,option_id,0)
            deadline=time.monotonic()+15
            while True:
                tabs=[x for x in windows(p.pid) if x[1]=='SysTabControl32']
                opts=[u.GetParent(x[0]) for x in tabs]
                if opts or time.monotonic()>deadline or p.poll() is not None: break
                time.sleep(.2)
            assert not any(x[2]=='Microsoft Visual C++ Runtime Library' for x in windows(p.pid)), 'Runtime assertion dialog appeared'
            assert opts, 'Options must open from taskbar command'
            tabs=[x for x in windows(p.pid) if x[1]=='SysTabControl32']
            assert len(tabs)==1, tabs
            assert u.SendMessageW(tabs[0][0],0x1304,0,0)==2, 'Expected exactly two options tabs'
            assert u.SendMessageW(tabs[0][0],0x130B,0,0)==0, 'Taskbar settings must be selected'
            u.PostMessageW(opts[0],0x111,2,0); time.sleep(.5)
        u.PostMessageW(core[0] if mode=='core' else display[0],0x10,0,0)
        code=p.wait(timeout=10)
        assert code==0, f'Exit failed: {code}'
        assert not windows(p.pid), 'Windows remained after exit'
        print(mode,'PASS',flush=True)
    finally:
        if p.poll() is None:
            p.terminate(); p.wait(timeout=5)

temporary.cleanup()
