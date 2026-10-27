"""Exercise a real Lite taskbar with isolated, synthetic AI cache data."""
import ctypes as C
from ctypes import wintypes as W
from pathlib import Path
import shutil, subprocess, sys, tempfile, time

u=C.WinDLL('user32',use_last_error=True)
g=C.WinDLL('gdi32',use_last_error=True)
u.SetProcessDPIAware() # Sample device pixels in the same coordinate system as the app.
u.GetDesktopWindow.restype=W.HWND
u.GetWindowThreadProcessId.argtypes=[W.HWND,C.POINTER(W.DWORD)]
u.GetWindowTextW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.GetClassNameW.argtypes=[W.HWND,W.LPWSTR,C.c_int]
u.GetClientRect.argtypes=[W.HWND,C.POINTER(W.RECT)]
u.GetDC.argtypes=[W.HWND];u.GetDC.restype=W.HDC
u.ReleaseDC.argtypes=[W.HWND,W.HDC]
u.PostMessageW.argtypes=[W.HWND,W.UINT,W.WPARAM,W.LPARAM]
g.GetPixel.argtypes=[W.HDC,C.c_int,C.c_int];g.GetPixel.restype=W.DWORD
callback=C.WINFUNCTYPE(W.BOOL,W.HWND,W.LPARAM)
u.EnumChildWindows.argtypes=[W.HWND,callback,W.LPARAM]

def find(pid):
    result={}
    @callback
    def visit(h,_):
        owner=W.DWORD();u.GetWindowThreadProcessId(h,C.byref(owner))
        if owner.value==pid:
            title=C.create_unicode_buffer(256);cls=C.create_unicode_buffer(256)
            u.GetWindowTextW(h,title,256);u.GetClassNameW(h,cls,256)
            if title.value=='TrafficMonitorTaskbarWindow':result['display']=h
            if cls.value=='TrafficMonitor_r7XZaS4p':result['core']=h
        return True
    u.EnumChildWindows(u.GetDesktopWindow(),visit,0)
    return result

def highlighted(h):
    rect=W.RECT();u.GetClientRect(h,C.byref(rect));dc=u.GetDC(h)
    try:
        xs=[x for x in range(2,rect.right,5) for y in range(2,rect.bottom,5)
            if g.GetPixel(dc,x,y)==(120 | (65<<8))]
        return len(xs), (min(xs),max(xs)) if xs else None, rect.right
    finally:u.ReleaseDC(h,dc)

exe=Path(sys.argv[1]).resolve()
assert exe.parent.name.startswith('Debug'), 'Use the isolated Debug mutex'
with tempfile.TemporaryDirectory(prefix='trafficmonitor-reset-') as temp:
    root=Path(temp);shutil.copy2(exe,root/'TrafficMonitor.exe')
    (root/'plugins').mkdir()
    (root/'plugins/AIUsage.ini').write_text('[settings]\nEnableQueries=0\n')
    (root/'config.ini').write_text('[general]\nmonitor_time_span=200\n[task_bar]\ntbar_display_item=0\nplugin_display_item=AIUsageCodexSummary,AIUsageClaudeSummary\nhorizontal_arrange=true\ntask_bar_back_color=1\nauto_set_background_color=false\ndisable_d2d=true\n[other]\nno_multistart_warning=true\n')
    cache=root/'plugins/AIUsage.cache.ini'
    def seed(reset):
        now=int(time.time())
        cache.write_text(f'[codex5h]\nvalue=17%\nstatus=OK\nupdated={now}\nresetEpoch={reset}\n[claude5h]\nvalue=54%\nstatus=OK\nupdated={now}\nresetEpoch={now+18000}\n',encoding='utf-8')
    seed(int(time.time())-100)
    process=subprocess.Popen([str(root/'TrafficMonitor.exe')],cwd=root)
    try:
        time.sleep(3);handles=find(process.pid)
        assert 'display' in handles and 'core' in handles, handles
        assert highlighted(handles['display'])[0]==0, 'Expired startup cache triggered flashing'
        reset=int(time.time())+4;seed(reset)
        counts=[];spans=[];first=None
        end=time.monotonic()+9
        while time.monotonic()<end:
            count,span,width=highlighted(handles['display'])
            if time.time()>=reset:
                counts.append(count)
                if count:
                    first=first or time.monotonic();spans.append(span)
                    assert span[1]-span[0]<width*.65, ('Unrelated Claude item was highlighted',span,width)
            time.sleep(.1)
        assert max(counts)>50 and 0 in counts, ('Background did not alternate',counts)
        print('PASS: expired startup suppressed; only Codex background alternates; Claude stays unchanged',flush=True)
        # Keep the expired cache unchanged and verify the alert expires, without re-triggering.
        end=first+31
        while time.monotonic()<end:
            time.sleep(min(.5,end-time.monotonic()))
        assert highlighted(handles['display'])[0]==0, 'Alert did not stop after 30 seconds'
        time.sleep(1)
        assert highlighted(handles['display'])[0]==0, 'Repeated stale cache re-triggered alert'
        print('PASS: background restored after 30 seconds; expired cache does not repeat',flush=True)
    finally:
        handles=find(process.pid)
        if 'core' in handles:u.PostMessageW(handles['core'],0x10,0,0)
        try:process.wait(timeout=10)
        except subprocess.TimeoutExpired:process.kill();process.wait();raise
