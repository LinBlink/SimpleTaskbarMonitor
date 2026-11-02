"""One-time migration of program-directory colors and display selection.
Runtime never reads AppData. An explicitly supplied old selection file is read-only.
"""
from pathlib import Path
import argparse, configparser, re

NORMAL_IDS=['up_string','down_string','cpu_string','memory_string','gpu_string','cpu_temp_string','gpu_temp_string','hdd_temp_string','main_board_temp_string','hdd_string','total_speed_string','cpu_freq_string','today_traffic_string']
AI_IDS=['AIUsageMiniMaxSummary','AIUsageCodexSummary','AIUsageCursorSummary','AIUsageClaudeSummary','AIUsageKimiSummary','AIUsageAntigravitySummary']
NATIVE_IDS=AI_IDS+['CustomDualCountdown','b5R30ITQ','BatteryPowerMon','BatteryPercentageMon','BatteryCapacityMon','BatteryVoltageMon','BatteryTimeMon']
CURRENT_IDS=[x for x in NORMAL_IDS if x not in ('hdd_temp_string','main_board_temp_string')]+NATIVE_IDS
OLD_V5_IDS=[x for x in NORMAL_IDS if x not in ('hdd_temp_string','main_board_temp_string')]+['AIUsageMiniMaxSummary','AIUsageCodexSummary','AIUsageCursorSummary','AIUsageClaudeSummary','AIUsageKimiSummary','AIUsageAntigravitySummary','AIUsageAntigravityClaudeSummary','AIUsageAntigravityArgonSummary','CustomDualCountdown','b5R30ITQ','BatteryPowerMon','BatteryPercentageMon','BatteryCapacityMon','BatteryVoltageMon','BatteryTimeMon']

CLEAN_STRINGS = {
    'task_bar': {
        'up_string': '"↑: "',
        'down_string': '"↓: "',
        'total_speed_string': '"↑↓: "',
        'gpu_string': '"GPU: "',
        'cpu_temp_string': '"CPU: "',
        'cpu_freq_string': '"FREQ: "',
        'gpu_temp_string': '"GPU: "',
        'hdd_temp_string': '"HDD: "',
        'main_board_temp_string': '"MBD: "',
        'hdd_string': '"HDD: "',
        'today_traffic_string': '"TRF: "',
    },
    'config': {
        'up_string': '"UP: "',
        'down_string': '"DN: "',
        'gpu_string': '"GPU: "',
        'gpu_temp_string': '"GPU: "',
        'hdd_temp_string': '"HDD: "',
        'main_board_temp_string': '"MBD: "',
        'hdd_string': '"HDD: "',
        'font_name': 'Segoe UI',
    },
    'main_window': {
        'up_string': '"UP: "',
        'down_string': '"DN: "',
        'gpu_string': '"GPU: "',
        'gpu_temp_string': '"GPU: "',
        'hdd_temp_string': '"HDD: "',
        'main_board_temp_string': '"MBD: "',
        'hdd_string': '"HDD: "',
        'font_name': 'Segoe UI',
    },
    'plugin_display_str_taskbar_window': {
        'b5R30ITQ': 'BAT:',
        'BatteryPowerMon': 'PWR:',
        'BatteryPercentageMon': 'BAT:',
        'BatteryCapacityMon': 'CAP:',
        'BatteryVoltageMon': 'VOL:',
        'BatteryTimeMon': 'TL:',
        'o282ffc4': 'Date',
        'ra1YX2g1': 'Time',
        'AIUsageMiniMaxWeek': '"MM W: "',
        'AIUsageCodexWeek': '"CX W: "',
        'AIUsageMiniMaxWeekReset': '"MM: "',
        'AIUsageCodexWeekReset': '"CX: "',
        'AIUsageCursorReset': '"CU: "',
        'AIUsageClaudeWeek': '"CL W: "',
        'AIUsageClaudeWeekReset': '"CL: "',
    },
    'plugin_display_str_main_window': {
        'b5R30ITQ': 'BAT:',
        'BatteryPowerMon': 'PW:',
        'BatteryPercentageMon': 'BAT:',
        'BatteryCapacityMon': 'CAP:',
        'BatteryVoltageMon': 'VOL:',
        'BatteryTimeMon': 'TL:',
        'o282ffc4': 'Date',
        'ra1YX2g1': 'Time',
    }
}

def summary_id(item):
    for provider in ['MiniMax','Codex','Cursor','Claude','Kimi','Antigravity']:
        if item.startswith('AIUsage'+provider):return 'AIUsage'+provider+'Summary'
    return item

def read(path):
    data=Path(path).read_bytes()
    if data.startswith(b'\xff\xfe'): encoding='utf-16'
    elif data.startswith(b'\xef\xbb\xbf'): encoding='utf-8-sig'
    else:
        try: data.decode('utf-8');encoding='utf-8'
        except UnicodeError: encoding='gb18030'
    text=data.decode(encoding)
    parser=configparser.ConfigParser(interpolation=None,strict=False)
    parser.optionxform=str;parser.read_string(text)
    return text,encoding,parser

def value(parser,section,key,default=''):
    return parser.get(section,key,fallback=default).strip().strip('"')

def entries(text,section):
    match=re.search(r'^\['+re.escape(section)+r'\]\s*\r?\n(.*?)(?=^\[|\Z)',text,re.M|re.S)
    return re.findall(r'^\s*([^;#\s=]+)\s*=',match[1],re.M) if match else []

def put(text,section,key,new_value):
    pattern=r'(^\['+re.escape(section)+r'\][^\r\n]*\r?\n)(.*?)(?=^\[|\Z)'
    match=re.search(pattern,text,re.M|re.S)
    newline='\r\n' if '\r\n' in text else '\n'
    line=key+' = '+str(new_value)+newline
    if not match: return text.rstrip() + newline+newline+'['+section+']'+newline+line
    body=match[2]
    key_pattern=r'^\s*'+re.escape(key)+r'\s*=[^\r\n]*(?:\r?\n|$)'
    body=re.sub(key_pattern,lambda _:line,body,flags=re.M) if re.search(key_pattern,body,re.M) else body.rstrip()+newline+line+newline
    return text[:match.start()]+match[1]+body+text[match.end():]

def migrate(path,selection_profile=None):
    path=Path(path);text,encoding,config=read(path)
    legacy=NORMAL_IDS+entries(text,'plugin_display_str_taskbar_window')
    for section in config.sections():
        if section not in ('task_bar','taskbar_default_style'): continue
        for key in config[section]:
            if key!='task_bar_text_color' and not re.fullmatch(r'default\d+_text_color',key): continue
            item_ids_str=value(config,section,key+'_item_ids')
            if not item_ids_str:
                numbers=value(config,section,key).rstrip(',').split(',')
                if len(numbers)%2 or len(numbers)//2!=len(legacy):
                    raise ValueError('Cannot safely infer legacy color IDs for '+section+'/'+key)
                text=put(text,section,key+'_item_ids',','.join(legacy))
            else:
                ids=item_ids_str.split(',')
                numbers=value(config,section,key).rstrip(',').split(',')
                if len(numbers)==2*len(ids):
                    changed=False
                    for target,donor in (('AIUsageKimiSummary','AIUsageClaudeSummary'),('AIUsageAntigravitySummary','AIUsageCursorSummary')):
                        if target in ids and donor in ids:
                            ti=ids.index(target);di=ids.index(donor)
                            if numbers[2*ti:2*ti+2]==['16777215','16777215'] and numbers[2*di:2*di+2]!=['16777215','16777215']:
                                numbers[2*ti:2*ti+2]=numbers[2*di:2*di+2]
                                changed=True
                    if changed:
                        text=put(text,section,key,','.join(numbers)+',')
    for ai_key,short_lbl in (('AIUsageMiniMaxSummary','MM'),('AIUsageCodexSummary','CX'),('AIUsageCursorSummary','CU'),('AIUsageClaudeSummary','CL'),('AIUsageKimiSummary','KI'),('AIUsageAntigravitySummary','AG')):
        if value(config,'plugin_display_str_taskbar_window',ai_key):
            text=put(text,'plugin_display_str_taskbar_window',ai_key,short_lbl)
    # Sanitize any mojibake or Chinese strings across known sections
    for sec, entries_dict in CLEAN_STRINGS.items():
        for k, clean_val in entries_dict.items():
            curr = value(config, sec, k)
            if curr:
                # Check for mojibake or Chinese or missing arrows
                is_arrow_key = k in ('up_string', 'down_string', 'total_speed_string')
                if sec == 'task_bar':
                    needs_clean = any(ord(c) > 127 for c in curr) or (is_arrow_key and ('↑' not in curr and '↓' not in curr))
                else:
                    needs_clean = any(ord(c) > 127 for c in curr)
                if needs_clean:
                    text=put(text, sec, k, clean_val)
    if value(config,'task_bar','font_name') == '微软雅黑':
        text=put(text,'task_bar','font_name','Segoe UI')
    selected=value(config,'task_bar','plugin_display_item').split(',')
    mask=int(value(config,'task_bar','tbar_display_item','3'))
    if selection_profile:
        _,_,old=read(selection_profile)
        selected+=value(old,'task_bar','plugin_display_item').split(',')
        mask|=int(value(old,'task_bar','tbar_display_item','0'))
    version=value(config,'app','builtin_configuration_version')
    selected={summary_id(x) for x in selected}
    if version not in ('6', '7'): selected.update(('AIUsageKimiSummary','AIUsageAntigravitySummary'))
    selected=[x for x in NATIVE_IDS if x in selected]
    text=put(text,'task_bar','plugin_display_item',','.join(selected))
    text=put(text,'task_bar','tbar_display_item',mask|(1<<5)|(1<<6))
    text=put(text,'general','hardware_monitor_item',int(value(config,'general','hardware_monitor_item','0'))|3)
    if version not in ('6', '7'):
        saved_ids=value(config,'task_bar','task_bar_text_color_item_ids')
        old_ids=OLD_V5_IDS if version=='5' else (CURRENT_IDS if version=='4' else (saved_ids.split(',') if saved_ids else legacy))
        order=[]
        for token in value(config,'task_bar','item_order').split(','):
            if not token.strip().isdigit():continue
            index=int(token)
            if index<len(old_ids) and summary_id(old_ids[index]) in CURRENT_IDS:
                new_index=CURRENT_IDS.index(summary_id(old_ids[index]))
                if new_index not in order:order.append(new_index)
        order += [i for i in range(len(CURRENT_IDS)) if i not in order]
        text=put(text,'task_bar','item_order',','.join(map(str,order)))
    text=put(text,'app','builtin_configuration_version','7')
    path.write_bytes(text.encode('utf-8-sig'))
    return len(selected)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('config',type=Path);parser.add_argument('--selection-profile',type=Path)
    args=parser.parse_args();count=migrate(args.config,args.selection_profile)
    print('Migrated configuration: legacy color values preserved; CPU/GPU temperature enabled; selected built-in items:',count)
