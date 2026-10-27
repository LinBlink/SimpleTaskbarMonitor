import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import argparse, re, subprocess, tempfile, time, shutil, hashlib

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
# Release installations may remain running; this test uses the separate Debug mutex.
resource_header=Path(__file__).resolve().parents[1]/'TrafficMonitor/resource.h'
header=resource_header.read_text(encoding='utf-8-sig')
option_id=int(re.search(r'#define ID_OPTIONS2\s+(\d+)',header)[1])
temporary=tempfile.TemporaryDirectory(prefix='trafficmonitor-smoke-')
work=Path(temporary.name)
program=work/'program';program.mkdir()
shutil.copy2(exe,program/'TrafficMonitor.exe')
exe=program/'TrafficMonitor.exe'
cwd=work/'cwd';cwd.mkdir()
(cwd/'config.ini').write_text('[sentinel]\nkeep=unchanged\n')
appdata=Path.home()/'AppData/Roaming/TrafficMonitor/config.ini'
appdata_before=hashlib.sha256(appdata.read_bytes()).digest() if appdata.exists() else None
for mode in ['core','display']:
    legacy='show_task_bar_wnd=false\n' if mode=='display' else ''
    (program/'global_cfg.ini').write_text('[config]\nportable_mode=false\n')
    (program/'config.ini').write_text('[config]\n'+legacy+'show_notify_icon=true\n[task_bar]\ntask_bar_text_color=123,456,987,654\ntask_bar_text_color_item_ids=AIUsageCodex5h,cpu_temp_string\nspecify_each_item_color=true\n[other]\nno_multistart_warning=true\n',encoding='utf-8')
    p=subprocess.Popen([str(exe)],cwd=cwd)
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
            feature_id=int(re.search(r'#define ID_PLUGIN_MANAGE\s+(\d+)',header)[1])
            refresh_id=int(re.search(r'#define ID_REFRESH_CONNECTION_LIST\s+(\d+)',header)[1])
            # Rebuild network menus repeatedly while the monitor thread is active.
            for _ in range(12):
                u.PostMessageW(core[0],0x111,refresh_id,0)
                time.sleep(.05)
            u.PostMessageW(core[0],0x111,feature_id,0)
            deadline=time.monotonic()+15
            while True:
                lists=[x for x in windows(p.pid) if x[1]=='SysListView32' and x[3]]
                if lists or time.monotonic()>deadline or p.poll() is not None: break
                time.sleep(.2)
            assert lists, 'Built-in features dialog did not open'
            assert u.SendMessageW(lists[0][0],0x1004,0,0)==4, 'Expected exactly four built-in features'
            u.PostMessageW(u.GetParent(lists[0][0]),0x111,2,0)
            time.sleep(.5)
        u.PostMessageW(core[0] if mode=='core' else display[0],0x10,0,0)
        code=p.wait(timeout=10)
        assert code==0, f'Exit failed: {code}'
        assert not windows(p.pid), 'Windows remained after exit'
        config=(program/'config.ini').read_text(encoding='utf-8-sig')
        assert 'task_bar_text_color_item_ids' in config, 'Colors must be saved with stable item IDs'
        ids=re.search(r'^task_bar_text_color_item_ids\s*=\s*"?([^"\r\n]+)',config,re.M)[1].split(',')
        numbers=re.search(r'^task_bar_text_color\s*=\s*([^\r\n]+)',config,re.M)[1].rstrip(',').split(',')
        restored={name:tuple(map(int,numbers[i*2:i*2+2])) for i,name in enumerate(ids)}
        assert restored['AIUsageCodex5h']==(123,456), 'AI text color changed'
        assert restored['cpu_temp_string']==(987,654), 'CPU temperature color changed'
        assert 'cpu_temp_string' in config and 'gpu_temp_string' in config, 'Lite must retain CPU/GPU temperature items'
        global_config=(program/'global_cfg.ini').read_text(encoding='utf-8-sig')
        assert re.search(r'portable_mode\s*=\s*true',global_config), 'Program directory must override old AppData settings'
        assert (cwd/'config.ini').read_text()=='[sentinel]\nkeep=unchanged\n', 'Working-directory config was touched'
        assert (hashlib.sha256(appdata.read_bytes()).digest() if appdata.exists() else None)==appdata_before, 'AppData config was touched'
        print(mode,'PASS: taskbar, colors by ID, CPU/GPU entries, program-directory config',flush=True)
    finally:
        if p.poll() is None:
            p.terminate(); p.wait(timeout=5)

temporary.cleanup()
