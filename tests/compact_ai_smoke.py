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
assert exe.parent.name.startswith('Debug')
with tempfile.TemporaryDirectory(prefix='trafficmonitor-compact-') as temp:
    root=Path(temp);shutil.copy2(exe,root/'TrafficMonitor.exe')
    (root/'plugins').mkdir()
    (root/'plugins/AIUsage.ini').write_text('[settings]\nEnableQueries=0\n')
    now=int(time.time())
    # Stale 5h above, fresh week below: exactly one label-side dot.
    (root/'plugins/AIUsage.cache.ini').write_text(f'[codex5h]\nvalue=17%\nstatus=offline\nupdated={now}\nresetEpoch={now+11160}\n[codexWeek]\nvalue=54%\nstatus=OK\nupdated={now}\nresetEpoch={now+360360}\n')
    sizes=[]
    for horizontal in ['true','false']:
        (root/'config.ini').write_text('[task_bar]\ntbar_display_item=0\nplugin_display_item=AIUsageCodexSummary\nhorizontal_arrange='+horizontal+'\ntask_bar_back_color=1\nauto_set_background_color=false\ndisable_d2d=true\ntask_bar_text_color=65280,16777215\ntask_bar_text_color_item_ids=AIUsageCodexSummary\nspecify_each_item_color=true\n[other]\nno_multistart_warning=true\n')
        process=subprocess.Popen([str(root/'TrafficMonitor.exe')],cwd=root)
        try:
            time.sleep(3);handles=find(process.pid)
            assert 'display' in handles and 'core' in handles
            h=handles['display'];rect=W.RECT();u.GetClientRect(h,C.byref(rect));dc=u.GetDC(h)
            try:
                colors=[[],[]];dots=[0,0]
                for y in range(rect.bottom):
                    row=int(y>=rect.bottom//2)
                    for x in range(rect.right):
                        color=g.GetPixel(dc,x,y)
                        if color==0xffffff:colors[row].append(x)
                        if color==(255 | (165<<8)):dots[row]+=1
                assert colors[0] and colors[1], 'Both quota rows must render values'
                assert dots[0]>0 and dots[1]==0, ('Stale indicator must belong only to the stale row',dots)
                assert abs(max(colors[0])-max(colors[1]))<=2, 'Reset times must share the same right edge'
                sizes.append((rect.right,rect.bottom))
            finally:u.ReleaseDC(h,dc)
            print('PASS: compact AI',horizontal,'size',sizes[-1],'; both periods visible, times aligned, stale row marked',flush=True)
        finally:
            handles=find(process.pid)
            if 'core' in handles:u.PostMessageW(handles['core'],0x10,0,0)
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait();raise
    assert abs(sizes[0][0]-sizes[1][0])<sizes[0][1]/2, ('AI must occupy its own column in either layout',sizes)
    assert sizes[0][1]==sizes[1][1], ('Horizontal mode must retain two full rows',sizes)
