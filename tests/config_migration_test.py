from pathlib import Path
import tempfile, sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from migrate_config import migrate, read, value, CURRENT_IDS, NORMAL_IDS
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder)/'config.ini'
    colors=','.join(str(x) for i in range(14) for x in (100+i,200+i))+','
    p.write_text('[app]\nversion=1.85.1\n[task_bar]\ntask_bar_text_color='+colors+'\nplugin_display_item=b5R30ITQ\ntbar_display_item=3\nitem_order=5,6,13,0,1\n[plugin_display_str_taskbar_window]\nb5R30ITQ=Battery\n',encoding='utf-8-sig')
    old=Path(folder)/'previous.ini'
    old.write_text('[task_bar]\nplugin_display_item=AIUsageCodex5h,AIUsageCodexWeekReset\ntbar_display_item=3\n')
    migrate(p,old)
    _,_,config=read(p)
    assert value(config,'task_bar','task_bar_text_color')==colors
    assert value(config,'task_bar','task_bar_text_color_item_ids').split(',')==NORMAL_IDS+['b5R30ITQ']
    assert int(value(config,'task_bar','tbar_display_item'))&96==96
    assert value(config,'task_bar','plugin_display_item').split(',').count('AIUsageCodexSummary')==1
    assert 'AIUsageKimiSummary' in value(config,'task_bar','plugin_display_item').split(',')
    assert 'AIUsageAntigravitySummary' in value(config,'task_bar','plugin_display_item').split(',')
    order=value(config,'task_bar','item_order').split(',')
    assert [CURRENT_IDS[int(x)] for x in order[:3]]==['cpu_temp_string','gpu_temp_string','b5R30ITQ']
    saved=p.read_bytes();migrate(p);assert p.read_bytes()==saved
print('PASS: legacy color values unchanged, metadata and order migrated, temperature selection restored, migration idempotent')
