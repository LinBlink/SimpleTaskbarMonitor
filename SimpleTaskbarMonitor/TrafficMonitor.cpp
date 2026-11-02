
// TrafficMonitor.cpp : 定义应用程序的类行为。
//

#include "stdafx.h"
#include "TrafficMonitor.h"
#include "TaskbarCore.h"
#include "Builtin/GroupedUsage.h"
#include "crashtool.h"
#include "WIC.h"
#include "auto_start_helper.h"
#include "AppAlreadyRuningDlg.h"
#include "WindowsSettingHelper.h"
#include "SettingsHelper.h"
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
#include "winrt/base.h"
#endif
#include <gdiplus.h>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// CTrafficMonitorApp

BEGIN_MESSAGE_MAP(CTrafficMonitorApp, CWinApp)
    //ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
    ON_COMMAND(ID_HELP, &CTrafficMonitorApp::OnHelp)
    ON_COMMAND(ID_FREQUENTY_ASKED_QUESTIONS, &CTrafficMonitorApp::OnFrequentyAskedQuestions)
    ON_COMMAND(ID_UPDATE_LOG, &CTrafficMonitorApp::OnUpdateLog)
END_MESSAGE_MAP()


CTrafficMonitorApp* CTrafficMonitorApp::self = NULL;


// CTrafficMonitorApp 构造
CTrafficMonitorApp::CTrafficMonitorApp()
{
    self = this;
    // 支持重新启动管理器
    //m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

    // TODO: 在此处添加构造代码，
    // 将所有重要的初始化放置在 InitInstance 中
    CRASHREPORT::StartCrashReport();
#ifndef DISABLE_WINDOWS_WEB_EXPERIENCE_DETECTOR
    if (m_win_version.IsWindows11OrLater())
        winrt::init_apartment();
#endif

    CheckWindows11Taskbar();
    m_theme_color = CCommon::GetWindowsThemeColor();
}


void CTrafficMonitorApp::LoadConfig()
{
    CSettingsHelper ini;

    //常规设置
    m_general_data.show_all_interface = ini.GetBool(L"general", L"show_all_interface", false);
    m_general_data.cpu_usage_acquire_method = static_cast<GeneralSettingData::CpuUsageAcquireMethod>(ini.GetInt(L"general", L"cpu_usage_acquire_method", GeneralSettingData::CA_PDH));
    m_general_data.monitor_time_span = ini.GetInt(L"general", L"monitor_time_span", 1000);
    if (m_general_data.monitor_time_span < MONITOR_TIME_SPAN_MIN || m_general_data.monitor_time_span > MONITOR_TIME_SPAN_MAX)
        m_general_data.monitor_time_span = 1000;
    m_general_data.hard_disk_name = ini.GetString(L"general", L"hard_disk_name", L"");
    m_general_data.cpu_core_name = ini.GetString(L"general", L"cpu_core_name", L"Core Average");
    m_general_data.hardware_monitor_item = ini.GetInt(L"general", L"hardware_monitor_item", 0);
    std::vector<std::wstring> connections_hide;
    ini.GetStringList(L"general", L"connections_hide", connections_hide, std::vector<std::wstring>{});
    m_general_data.connections_hide.FromVector(connections_hide);

    //Windows10颜色模式设置
    bool is_windows10_light_theme = CWindowsSettingHelper::IsWindows10LightTheme();
    if (is_windows10_light_theme)
        CCommon::SetColorMode(ColorMode::Light);
    else
        CCommon::SetColorMode(ColorMode::Default);

    //任务栏窗口设置
    m_general_data.show_notify_icon = ini.GetBool(_T("config"), _T("show_notify_icon"), true);
    m_cfg_data.m_auto_select = ini.GetBool(_T("connection"), _T("auto_select"), true);
    m_cfg_data.m_select_all = ini.GetBool(_T("connection"), _T("select_all"), false);
    m_cfg_data.m_connection_name = CCommon::UnicodeToStr(ini.GetString(L"connection", L"connection_name", L"").c_str());

    m_cfg_data.m_notify_icon_selected = ini.GetInt(_T("config"), _T("notify_icon_selected"), (m_win_version.IsWindows7() || m_win_version.IsWindows8Or8point1() ? 2 : m_cfg_data.m_dft_notify_icon));       //Win7/8/8.1默认使用蓝色通知区图标，因为隐藏通知区图标后白色图标会看不清，其他系统默认使用白色图标
    m_cfg_data.m_notify_icon_auto_adapt = ini.GetBool(_T("config"), _T("notify_icon_auto_adapt"), true);
    if (m_cfg_data.m_notify_icon_auto_adapt)
        AutoSelectNotifyIcon();

    m_general_data.traffic_tip_enable = ini.GetBool(L"notify_tip", L"traffic_tip_enable", false);
    m_general_data.traffic_tip_value = ini.GetInt(L"notify_tip", L"traffic_tip_value", 200);
    m_general_data.traffic_tip_unit = ini.GetInt(L"notify_tip", L"traffic_tip_unit", 0);
    m_general_data.memory_usage_tip.enable = ini.GetBool(L"notify_tip", L"memory_usage_tip_enable", false);
    m_general_data.memory_usage_tip.tip_value = ini.GetInt(L"notify_tip", L"memory_tip_value", 80);
    m_general_data.cpu_temp_tip.enable = ini.GetBool(L"notify_tip", L"cpu_temperature_tip_enable", false);
    m_general_data.cpu_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"cpu_temperature_tip_value", 80);
    m_general_data.gpu_temp_tip.enable = ini.GetBool(L"notify_tip", L"gpu_temperature_tip_enable", false);
    m_general_data.gpu_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"gpu_temperature_tip_value", 80);
    m_general_data.hdd_temp_tip.enable = ini.GetBool(L"notify_tip", L"hdd_temperature_tip_enable", false);
    m_general_data.hdd_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"hdd_temperature_tip_value", 80);
    m_general_data.mainboard_temp_tip.enable = ini.GetBool(L"notify_tip", L"mainboard_temperature_tip_enable", false);
    m_general_data.mainboard_temp_tip.tip_value = ini.GetInt(L"notify_tip", L"mainboard_temperature_tip_value", 80);

    //任务栏窗口设置
    m_taskbar_data.back_color = ini.GetInt(_T("task_bar"), _T("task_bar_back_color"), m_taskbar_data.dft_back_color);
    m_taskbar_data.transparent_color = ini.GetInt(_T("task_bar"), _T("transparent_color"), m_taskbar_data.dft_transparent_color);
    if (m_taskbar_data.IsTaskbarTransparent()) //如果任务栏背景透明，则需要将颜色转换一下
    {
        CCommon::TransparentColorConvert(m_taskbar_data.back_color);
        CCommon::TransparentColorConvert(m_taskbar_data.transparent_color);
    }
    m_taskbar_data.status_bar_color = ini.GetInt(_T("task_bar"), _T("status_bar_color"), m_taskbar_data.dft_status_bar_color);
    //m_taskbar_data.text_color = GetPrivateProfileInt(_T("task_bar"), _T("task_bar_text_color"), 0x00ffffffU, m_config_path.c_str());
    ini.LoadTaskbarWndColors(_T("task_bar"), _T("task_bar_text_color"), m_taskbar_data.text_colors, m_taskbar_data.dft_text_colors);
    m_taskbar_data.specify_each_item_color = ini.GetBool(L"task_bar", L"specify_each_item_color", false);
    //m_cfg_data.m_tbar_show_cpu_memory = ini.GetBool(_T("task_bar"), _T("task_bar_show_cpu_memory"), false);
    m_taskbar_data.display_item.FromInt(ini.GetInt(L"task_bar", L"tbar_display_item", DisplayItemSet{ TDI_UP, TDI_DOWN }.ToInt()));
    m_taskbar_data.show_taskbar_wnd_in_secondary_display = ini.GetBool(L"task_bar", L"show_taskbar_wnd_in_secondary_display", false);
    m_taskbar_data.secondary_display_index = ini.GetInt(L"task_bar", L"secondary_display_index", 0);

#ifdef WITHOUT_TEMPERATURE
    m_taskbar_data.display_item.Remove(TDI_HDD_TEMP);
    m_taskbar_data.display_item.Remove(TDI_MAIN_BOARD_TEMP);
#endif

    //如果选项设置中关闭了某个硬件监控，则不显示对应的温度监控相关项目
    if (!m_general_data.IsHardwareEnable(HI_CPU))
        m_taskbar_data.display_item.Remove(TDI_CPU_TEMP);
    if (!m_general_data.IsHardwareEnable(HI_GPU))
    {
        m_taskbar_data.display_item.Remove(TDI_GPU_TEMP);
    }
    if (!m_general_data.IsHardwareEnable(HI_HDD))
    {
        m_taskbar_data.display_item.Remove(TDI_HDD_TEMP);
    }
    if (!m_general_data.IsHardwareEnable(HI_MBD))
        m_taskbar_data.display_item.Remove(TDI_MAIN_BOARD_TEMP);

    //m_taskbar_data.swap_up_down = ini.GetBool(_T("task_bar"), _T("task_bar_swap_up_down"), false);

    if (m_taskbar_data.back_color == 0 && !m_taskbar_data.text_colors.empty() && m_taskbar_data.text_colors.begin()->second.label == 0)     //万一读取到的背景色和文本颜色都为0（黑色），则将文本色和背景色设置成默认颜色
    {
        m_taskbar_data.back_color = m_taskbar_data.dft_back_color;
        m_taskbar_data.text_colors.begin()->second.label = m_taskbar_data.dft_text_colors;
    }

    FontInfo default_font;
    default_font.name = m_str_table.GetLanguageInfo().default_font_name.c_str();
    default_font.size = 9;
    ini.LoadFontData(_T("task_bar"), m_taskbar_data.font, default_font);

    //载入显示文本设置
    ini.LoadDisplayStr(L"task_bar", m_taskbar_data.disp_str, false);
    ini.LoadPluginDisplayStr(L"plugin_display_str_taskbar_window", m_taskbar_data.disp_str, false);

    m_taskbar_data.tbar_wnd_on_left = ini.GetBool(_T("task_bar"), _T("task_bar_wnd_on_left"), false);
    m_taskbar_data.speed_short_mode = ini.GetBool(_T("task_bar"), _T("task_bar_speed_short_mode"), true);
    m_taskbar_data.tbar_wnd_snap = ini.GetBool(_T("task_bar"), _T("task_bar_wnd_snap"), false);
    m_taskbar_data.unit_byte = ini.GetBool(_T("task_bar"), _T("unit_byte"), true);
    m_taskbar_data.speed_unit = static_cast<SpeedUnit>(ini.GetInt(_T("task_bar"), _T("task_bar_speed_unit"), 0));
    m_taskbar_data.hide_unit = ini.GetBool(_T("task_bar"), _T("task_bar_hide_unit"), false);
    m_taskbar_data.hide_percent = ini.GetBool(_T("task_bar"), _T("task_bar_hide_percent"), false);
    m_taskbar_data.value_right_align = ini.GetBool(_T("task_bar"), _T("value_right_align"), true);
    m_taskbar_data.horizontal_arrange = ini.GetBool(_T("task_bar"), _T("horizontal_arrange"), false);
    m_taskbar_data.show_status_bar = ini.GetBool(_T("task_bar"), _T("show_status_bar"), true);
    m_taskbar_data.separate_value_unit_with_space = ini.GetBool(_T("task_bar"), _T("separate_value_unit_with_space"), true);
    m_taskbar_data.show_tool_tip = ini.GetBool(_T("task_bar"), _T("show_tool_tip"), true);
    m_taskbar_data.digits_number = ini.GetInt(_T("task_bar"), _T("digits_number"), 4);
    m_taskbar_data.memory_display = static_cast<MemoryDisplay>(ini.GetInt(L"task_bar", L"memory_display", static_cast<int>(MemoryDisplay::USAGE_PERCENTAGE)));
    m_taskbar_data.double_click_action = static_cast<DoubleClickAction>(ini.GetInt(_T("task_bar"), _T("double_click_action"), 0));
    m_taskbar_data.double_click_exe = ini.GetString(L"task_bar", L"double_click_exe", (theApp.m_system_dir + L"\\Taskmgr.exe").c_str());
    m_taskbar_data.cm_graph_type = ini.GetBool(_T("task_bar"), _T("cm_graph_type"), true);
    m_taskbar_data.show_graph_dashed_box = ini.GetBool(L"task_bar", L"show_graph_dashed_box", false);
    m_taskbar_data.item_space = ini.GetInt(L"task_bar", L"item_space", 8);
    m_taskbar_data.vertical_margin = ini.GetInt(L"task_bar", L"vertical_margin", 0);
    m_taskbar_data.window_offset_top = ini.GetInt(L"task_bar", L"window_offset_top", 0);
    m_taskbar_data.window_offset_left = ini.GetInt(L"task_bar", L"window_offset_left", 0);
    m_taskbar_data.ValidItemSpace();
    m_taskbar_data.ValidWindowOffsetTop();
    m_taskbar_data.ValidWindowOffsetLeft();
    m_taskbar_data.avoid_overlap_with_widgets = ini.GetBool(_T("task_bar"), _T("avoid_overlap_with_widgets"), false);
    m_taskbar_data.taskbar_left_space_win11 = ini.GetInt(L"task_bar", L"taskbar_left_space_win11", 160);
    m_taskbar_data.taskbar_right_space_win11 = ini.GetInt(L"task_bar", L"taskbar_right_space_win11", 280);

    if (m_win_version.IsWindows10OrLater())     //只有Win10才支持自动适应系统深色/浅色主题
        m_taskbar_data.auto_adapt_light_theme = ini.GetBool(L"task_bar", L"auto_adapt_light_theme", false);
    else
        m_taskbar_data.auto_adapt_light_theme = false;
    m_taskbar_data.dark_default_style = ini.GetInt(L"task_bar", L"dark_default_style", 0);
    m_taskbar_data.light_default_style = ini.GetInt(L"task_bar", L"light_default_style", TASKBAR_DEFAULT_LIGHT_STYLE_INDEX);

    if (m_win_version.IsWindows8OrLater())
        m_taskbar_data.auto_set_background_color = ini.GetBool(L"task_bar", L"auto_set_background_color", false);
    else
        m_taskbar_data.auto_set_background_color = false;

    m_taskbar_data.item_order.Init();
    m_taskbar_data.item_order.FromString(ini.GetString(L"task_bar", L"item_order", L""));
    std::vector<std::wstring> displayed_ids;
    CCommon::StringSplit(ini.GetString(L"task_bar",L"plugin_display_item",L""),L',',displayed_ids);
    std::wstring grouped_ids;
    for(const auto& id:displayed_ids) {
        if(!grouped_ids.empty()) grouped_ids+=L",";
        grouped_ids+=GroupedUsage::SummaryId(id);
    }
    m_taskbar_data.plugin_display_item.FromString(grouped_ids);
    m_taskbar_data.auto_save_taskbar_color_settings_to_preset = ini.GetBool(L"task_bar", L"auto_save_taskbar_color_settings_to_preset", true);

    m_taskbar_data.show_netspeed_figure = ini.GetBool(L"task_bar", L"show_netspeed_figure", true);
    m_taskbar_data.netspeed_figure_max_value = ini.GetInt(L"task_bar", L"netspeed_figure_max_value", 10);
    m_taskbar_data.netspeed_figure_max_value_unit = ini.GetInt(L"task_bar", L"netspeed_figure_max_value_unit", 1);
    m_taskbar_data.graph_color_following_system = ini.GetBool(L"task_bar", L"graph_color_following_system", true);

    if (CTaskBarDlgDrawCommonSupport::CheckSupport())
        m_taskbar_data.disable_d2d = ini.GetBool(L"task_bar", L"disable_d2d", true);
    else
        m_taskbar_data.disable_d2d = true;
    m_taskbar_data.enable_colorful_emoji = ini.GetBool(L"task_bar", L"enable_colorful_emoji", true);

    //其他设置
    //m_cfg_data.m_show_internet_ip = ini.GetBool(L"connection_details", L"show_internet_ip", false);

    m_notify_interval = ini.GetInt(_T("other"), _T("notify_interval"), 60);
    m_exit_when_start_by_restart_manager = ini.GetBool(_T("other"), _T("exit_when_start_by_restart_manager"), true);
    m_debug_log = ini.GetBool(_T("other"), _T("debug_log"), false);
    //由于Win7系统中设置任务栏窗口透明色会导致任务栏窗口不可见，因此默认在Win7中禁用透明色的设定
    m_taksbar_transparent_color_enable = ini.GetBool(L"other", L"taksbar_transparent_color_enable", !m_win_version.IsWindows7());
    m_last_light_mode = ini.GetBool(L"other", L"last_light_mode", CWindowsSettingHelper::IsWindows10LightTheme());
    m_show_mouse_panetrate_tip = ini.GetBool(L"other", L"show_mouse_panetrate_tip", true);
    m_show_dot_net_notinstalled_tip = ini.GetBool(L"other", L"show_dot_net_notinstalled_tip", true);
}

void CTrafficMonitorApp::SaveConfig()
{
    CSettingsHelper ini;

    //常规设置
    ini.WriteBool(L"general", L"show_all_interface", m_general_data.show_all_interface);
    ini.WriteInt(L"general", L"cpu_usage_acquire_method", m_general_data.cpu_usage_acquire_method);
    ini.WriteInt(L"general", L"monitor_time_span", m_general_data.monitor_time_span);
    ini.WriteString(L"general", L"hard_disk_name", m_general_data.hard_disk_name);
    ini.WriteString(L"general", L"cpu_core_name", m_general_data.cpu_core_name);
    ini.WriteInt(L"general", L"hardware_monitor_item", m_general_data.hardware_monitor_item);
    ini.WriteStringList(L"general", L"connections_hide", m_general_data.connections_hide.ToVector());

    //任务栏窗口设置
    ini.WriteBool(L"config", L"show_notify_icon", m_general_data.show_notify_icon);
    ini.WriteBool(L"connection", L"auto_select", m_cfg_data.m_auto_select);
    ini.WriteBool(L"connection", L"select_all", m_cfg_data.m_select_all);
    ini.WriteString(L"connection", L"connection_name", CCommon::StrToUnicode(m_cfg_data.m_connection_name.c_str()));

    ini.WriteInt(L"config", L"notify_icon_selected", m_cfg_data.m_notify_icon_selected);
    ini.WriteBool(L"config", L"notify_icon_auto_adapt", m_cfg_data.m_notify_icon_auto_adapt);

    ini.WriteBool(L"notify_tip", L"traffic_tip_enable", m_general_data.traffic_tip_enable);
    ini.WriteInt(L"notify_tip", L"traffic_tip_value", m_general_data.traffic_tip_value);
    ini.WriteInt(L"notify_tip", L"traffic_tip_unit", m_general_data.traffic_tip_unit);
    ini.WriteBool(L"notify_tip", L"memory_usage_tip_enable", m_general_data.memory_usage_tip.enable);
    ini.WriteInt(L"notify_tip", L"memory_tip_value", m_general_data.memory_usage_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"cpu_temperature_tip_enable", m_general_data.cpu_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"cpu_temperature_tip_value", m_general_data.cpu_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"gpu_temperature_tip_enable", m_general_data.gpu_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"gpu_temperature_tip_value", m_general_data.gpu_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"hdd_temperature_tip_enable", m_general_data.hdd_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"hdd_temperature_tip_value", m_general_data.hdd_temp_tip.tip_value);
    ini.WriteBool(L"notify_tip", L"mainboard_temperature_tip_enable", m_general_data.mainboard_temp_tip.enable);
    ini.WriteInt(L"notify_tip", L"mainboard_temperature_tip_value", m_general_data.mainboard_temp_tip.tip_value);

    //任务栏窗口设置
    ini.WriteInt(L"task_bar", L"task_bar_back_color", m_taskbar_data.back_color);
    ini.WriteInt(L"task_bar", L"transparent_color", m_taskbar_data.transparent_color);
    ini.WriteInt(L"task_bar", L"status_bar_color", m_taskbar_data.status_bar_color);
    ini.SaveTaskbarWndColors(L"task_bar", L"task_bar_text_color", m_taskbar_data.text_colors);
    ini.WriteBool(L"task_bar", L"specify_each_item_color", m_taskbar_data.specify_each_item_color);
    //ini.WriteBool(L"task_bar", L"task_bar_show_cpu_memory", m_cfg_data.m_tbar_show_cpu_memory);
    ini.WriteInt(L"task_bar", L"tbar_display_item", m_taskbar_data.display_item.ToInt());
    ini.SaveFontData(L"task_bar", m_taskbar_data.font);
    //ini.WriteBool(L"task_bar", L"task_bar_swap_up_down", m_taskbar_data.swap_up_down);
    ini.WriteBool(L"task_bar", L"show_taskbar_wnd_in_secondary_display", m_taskbar_data.show_taskbar_wnd_in_secondary_display);
    ini.WriteInt(L"task_bar", L"secondary_display_index", m_taskbar_data.secondary_display_index);

    ini.SaveDisplayStr(L"task_bar", m_taskbar_data.disp_str);
    ini.SavePluginDisplayStr(L"plugin_display_str_taskbar_window", m_taskbar_data.disp_str);

    ini.WriteBool(L"task_bar", L"task_bar_wnd_on_left", m_taskbar_data.tbar_wnd_on_left);
    ini.WriteBool(L"task_bar", L"task_bar_wnd_snap", m_taskbar_data.tbar_wnd_snap);
    ini.WriteBool(L"task_bar", L"task_bar_speed_short_mode", m_taskbar_data.speed_short_mode);
    ini.WriteBool(L"task_bar", L"unit_byte", m_taskbar_data.unit_byte);
    ini.WriteInt(L"task_bar", L"task_bar_speed_unit", static_cast<int>(m_taskbar_data.speed_unit));
    ini.WriteBool(L"task_bar", L"task_bar_hide_unit", m_taskbar_data.hide_unit);
    ini.WriteBool(L"task_bar", L"task_bar_hide_percent", m_taskbar_data.hide_percent);
    ini.WriteBool(L"task_bar", L"value_right_align", m_taskbar_data.value_right_align);
    ini.WriteBool(L"task_bar", L"horizontal_arrange", m_taskbar_data.horizontal_arrange);
    ini.WriteBool(L"task_bar", L"show_status_bar", m_taskbar_data.show_status_bar);
    ini.WriteBool(L"task_bar", L"separate_value_unit_with_space", m_taskbar_data.separate_value_unit_with_space);
    ini.WriteBool(L"task_bar", L"show_tool_tip", m_taskbar_data.show_tool_tip);
    ini.WriteInt(L"task_bar", L"digits_number", m_taskbar_data.digits_number);
    ini.WriteInt(L"task_bar", L"memory_display", static_cast<int>(m_taskbar_data.memory_display));
    ini.WriteInt(L"task_bar", L"double_click_action", static_cast<int>(m_taskbar_data.double_click_action));
    ini.WriteString(L"task_bar", L"double_click_exe", m_taskbar_data.double_click_exe);
    ini.WriteBool(L"task_bar", L"cm_graph_type", m_taskbar_data.cm_graph_type);
    ini.WriteBool(L"task_bar", L"show_graph_dashed_box", m_taskbar_data.show_graph_dashed_box);
    ini.WriteInt(L"task_bar", L"item_space", m_taskbar_data.item_space);
    ini.WriteInt(L"task_bar", L"vertical_margin", m_taskbar_data.vertical_margin);
    ini.WriteInt(L"task_bar", L"window_offset_top", m_taskbar_data.window_offset_top);
    ini.WriteInt(L"task_bar", L"window_offset_left", m_taskbar_data.window_offset_left);
    ini.WriteBool(L"task_bar", L"avoid_overlap_with_widgets", m_taskbar_data.avoid_overlap_with_widgets);
    ini.WriteInt(L"task_bar", L"taskbar_left_space_win11", m_taskbar_data.taskbar_left_space_win11);
    ini.WriteInt(L"task_bar", L"taskbar_right_space_win11", m_taskbar_data.taskbar_right_space_win11);

    ini.WriteBool(L"task_bar", L"auto_adapt_light_theme", m_taskbar_data.auto_adapt_light_theme);
    ini.WriteInt(L"task_bar", L"dark_default_style", m_taskbar_data.dark_default_style);
    ini.WriteInt(L"task_bar", L"light_default_style", m_taskbar_data.light_default_style);
    ini.WriteBool(L"task_bar", L"auto_set_background_color", m_taskbar_data.auto_set_background_color);

    ini.WriteString(L"task_bar", L"item_order", m_taskbar_data.item_order.ToString());
    ini.WriteString(L"task_bar", L"plugin_display_item", m_taskbar_data.plugin_display_item.ToString());
    ini.WriteBool(L"task_bar", L"auto_save_taskbar_color_settings_to_preset", m_taskbar_data.auto_save_taskbar_color_settings_to_preset);

    ini.WriteBool(L"task_bar", L"show_netspeed_figure", m_taskbar_data.show_netspeed_figure);
    ini.WriteInt(L"task_bar", L"netspeed_figure_max_value", m_taskbar_data.netspeed_figure_max_value);
    ini.WriteInt(L"task_bar", L"netspeed_figure_max_value_unit", m_taskbar_data.netspeed_figure_max_value_unit);
    ini.WriteBool(L"task_bar", L"graph_color_following_system", m_taskbar_data.graph_color_following_system);

    ini.WriteBool(L"task_bar", L"disable_d2d", m_taskbar_data.disable_d2d);
    ini.WriteBool(L"task_bar", L"enable_colorful_emoji", m_taskbar_data.enable_colorful_emoji);

    //其他设置

    ini.WriteBool(_T("other"), _T("no_multistart_warning"), m_no_multistart_warning);
    ini.WriteBool(_T("other"), _T("exit_when_start_by_restart_manager"), m_exit_when_start_by_restart_manager);
    ini.WriteBool(_T("other"), _T("debug_log"), m_debug_log);
    ini.WriteInt(_T("other"), _T("notify_interval"), m_notify_interval);
    ini.WriteBool(_T("other"), _T("taksbar_transparent_color_enable"), m_taksbar_transparent_color_enable);
    ini.WriteBool(_T("other"), _T("last_light_mode"), m_last_light_mode);
    ini.WriteBool(_T("other"), _T("show_mouse_panetrate_tip"), m_show_mouse_panetrate_tip);
    ini.WriteBool(_T("other"), _T("show_dot_net_notinstalled_tip"), m_show_dot_net_notinstalled_tip);

    ini.WriteString(L"config", L"plugin_disabled", m_cfg_data.plugin_disabled.ToString());

    ini.WriteString(L"app", L"version", VERSION);

    //检查是否保存成功
    if (!ini.Save())
    {
        if (m_cannot_save_config_warning)
        {
            CString info = CCommon::LoadText(IDS_CONNOT_SAVE_CONFIG_WARNING);
            info.Replace(_T("<%file_path%>"), m_config_path.c_str());
            AfxMessageBox(info, MB_ICONWARNING);
        }
        m_cannot_save_config_warning = false;
        return;
    }
}

void CTrafficMonitorApp::LoadPluginDisabledSettings()
{
    CIniHelper ini{ m_config_path };
    m_cfg_data.plugin_disabled.FromString(ini.GetString(L"config", L"plugin_disabled", L""));
}

void CTrafficMonitorApp::LoadGlobalConfig()
{
    // Configuration always belongs to this executable, including Debug builds.
    m_general_data.portable_mode = true;
    CIniHelper ini{ m_module_dir + L"global_cfg.ini" };
    ini.WriteBool(L"config", L"portable_mode", true);
    m_module_dir_writable = ini.Save();
}

void CTrafficMonitorApp::SaveGlobalConfig()
{
    CIniHelper ini{ m_module_dir + L"global_cfg.ini" };
    ini.WriteBool(L"config", L"portable_mode", true);

    //检查是否保存成功
    if (!ini.Save())
    {
        //if (m_cannot_save_global_config_warning)
        //{
        //    CString info;
        //    info.LoadString(IDS_CONNOT_SAVE_CONFIG_WARNING);
        //    info.Replace(_T("<%file_path%>"), m_module_dir.c_str());
        //    AfxMessageBox(info, MB_ICONWARNING);
        //}
        //m_cannot_save_global_config_warning = false;
        //return;
    }
}

int CTrafficMonitorApp::DPI(int pixel)
{
    return m_dpi * pixel / 96;
}

void CTrafficMonitorApp::DPI(CRect& rect)
{
    rect.left = DPI(rect.left);
    rect.right = DPI(rect.right);
    rect.top = DPI(rect.top);
    rect.bottom = DPI(rect.bottom);
}

void CTrafficMonitorApp::DPIFromWindow(CWnd* pWnd)
{
    CWindowDC dc(pWnd);
    HDC hDC = dc.GetSafeHdc();
    m_dpi = GetDeviceCaps(hDC, LOGPIXELSY);
}


UINT CTrafficMonitorApp::InitOpenHardwareMonitorLibThreadFunc(LPVOID lpParam)
{
#ifndef WITHOUT_TEMPERATURE
    CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
    theApp.m_pMonitor = OpenHardwareMonitorApi::CreateInstance();
    if (theApp.m_pMonitor == nullptr)
    {
        AfxMessageBox(OpenHardwareMonitorApi::GetErrorMessage().c_str(), MB_ICONERROR | MB_OK);
    }
    //设置硬件监控的启用状态
    theApp.UpdateOpenHardwareMonitorEnableState();
#endif
    return 0;
}

bool  CTrafficMonitorApp::SetAutoRun(bool auto_run, bool task_scheduler)
{
    if (task_scheduler)
    {
        //使用任务计划的方式设置开机自启动
        return SetAutoRunByTaskScheduler(auto_run);
    }
    else
    {
        //使用添加注册表项的方式实现开机自启动
        return SetAutoRunByRegistry(auto_run);
    }
}

bool CTrafficMonitorApp::GetAutoRun(wstring* auto_run_path, bool task_scheduler)
{
    if (auto_run_path != nullptr)
        auto_run_path->clear();

    if (task_scheduler)
    {
        //使用任务计划的方式实现开机自启动
        return is_auto_start_task_active_for_this_user(auto_run_path);
    }
    else
    {
        //使用添加注册表项的方式实现开机自启动
        CRegKey key;
        if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
        {
            //打开注册表“Software\\Microsoft\\Windows\\CurrentVersion\\Run”失败，则返回false
            return false;
        }
        wchar_t buff[256];
        ULONG size{ 256 };
        if (key.QueryStringValue(APP_NAME, buff, &size) == ERROR_SUCCESS)       //如果找到了“TrafficMonitor”键
        {
            if (auto_run_path != nullptr)
            {
                //保存路径
                *auto_run_path = buff;
                //去掉前后的引号
                if (auto_run_path->front() == L'\"')
                    *auto_run_path = auto_run_path->substr(1);
                if (auto_run_path->back() = L'\"')
                    auto_run_path->pop_back();
            }
            return (m_module_path_reg == buff); //如果“TrafficMonitor”的值是当前程序的路径，就返回true，否则返回false
        }
        else
        {
            return false;       //没有找到“TrafficMonitor”键，返回false
        }
    }
}

bool CTrafficMonitorApp::SetAutoRunByRegistry(bool auto_run)
{
    CRegKey key;
    //打开注册表项
    if (key.Open(HKEY_CURRENT_USER, _T("Software\\Microsoft\\Windows\\CurrentVersion\\Run")) != ERROR_SUCCESS)
    {
        AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_FAILED_NO_KEY), MB_OK | MB_ICONWARNING);
        return false;
    }
    if (auto_run)       //写入注册表项
    {
        //通过注册表设置开机自启动项时删除计划任务中的自启动项
        SetAutoRunByTaskScheduler(false);

        if (key.SetStringValue(APP_NAME, m_module_path_reg.c_str()) != ERROR_SUCCESS)
        {
            AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_FAILED_NO_ACCESS), MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    else        //删除注册表项
    {
        //删除前先检查注册表项是否存在，如果不存在，则直接返回
        wchar_t buff[256];
        ULONG size{ 256 };
        if (key.QueryStringValue(APP_NAME, buff, &size) != ERROR_SUCCESS)
            return false;
        if (key.DeleteValue(APP_NAME) != ERROR_SUCCESS)
        {
            AfxMessageBox(CCommon::LoadText(IDS_AUTORUN_DELETE_FAILED), MB_OK | MB_ICONWARNING);
            return false;
        }
    }
    return true;
}

bool CTrafficMonitorApp::SetAutoRunByTaskScheduler(bool auto_run)
{
    bool succeed = delete_auto_start_task_for_this_user();     //先删除开机自启动
    if (auto_run)
    {
        //通过计划任务设置开机自启动项时删除注册表中的自启动项
        SetAutoRunByRegistry(false);

        succeed = create_auto_start_task_for_this_user(true);
    }
    return succeed;
}

CString CTrafficMonitorApp::GetSystemInfoString()
{
    CString info;
    info += _T("System Info:\r\n");

    CString strTmp;
    strTmp.Format(_T("Windows Version: %d.%d build %d"), m_win_version.GetMajorVersion(),
        m_win_version.GetMinorVersion(), m_win_version.GetBuildNumber());
    info += strTmp;

    if (m_win_version.IsWine())
        info += _T(" (Wine)");

    info += _T("\r\n");

    strTmp.Format(_T("DPI: %d"), m_dpi);
    info += strTmp;
    info += _T("\r\n");

    info += _T("Version: ");
    info += VERSION;
    info += _T(" ");
#ifdef _M_X64
    info += _T("x64");
#elif defined _M_ARM64EC
    info += _T("Arm64EC");
#else
    info += _T("x86");
#endif

#ifdef WITHOUT_TEMPERATURE
    info += _T(" (Lite)");
#endif

    info += _T("\r\nLast compiled date: ");
    info += CCommon::GetLastCompileTime();

    return info;
}


void CTrafficMonitorApp::InitMenuResourse()
{
    //载入菜单
    CCommon::LoadMenuResource(m_taskbar_menu, IDR_TASK_BAR_MENU);
    CCommon::LoadMenuResource(m_taskbar_menu_plugin, IDR_TASK_BAR_MENU);

    //为插件菜单添加额外项目
    m_taskbar_menu_plugin_sub_menu.CreatePopupMenu();
    m_taskbar_menu_plugin_sub_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_OPTIONS_TASKBAR, CCommon::LoadText(IDS_PLUGIN_OPTIONS, _T("...")));
    m_taskbar_menu_plugin_sub_menu.AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_DETAIL_TASKBAR, CCommon::LoadText(IDS_PLUGIN_INFO, _T("...")));
    CMenuIcon::AddIconToMenuItem(m_taskbar_menu_plugin_sub_menu.GetSafeHmenu(), ID_PLUGIN_OPTIONS_TASKBAR, FALSE, GetMenuIcon(IDI_SETTINGS));
    CMenuIcon::AddIconToMenuItem(m_taskbar_menu_plugin_sub_menu.GetSafeHmenu(), ID_PLUGIN_DETAIL_TASKBAR, FALSE, GetMenuIcon(IDI_INFO));
    CMenu* taskbar_menu_plugin = m_taskbar_menu_plugin.GetSubMenu(0);
    taskbar_menu_plugin->AppendMenu(MF_SEPARATOR);
    taskbar_menu_plugin->AppendMenu(MF_POPUP | MF_STRING, (UINT_PTR)m_taskbar_menu_plugin_sub_menu.m_hMenu, _T("<plugin name>"));

    //为菜单项添加图标
    //任务栏窗口右键菜单
    auto addIconsForTaksbarWindowMenu = [&](const CMenu& menu)
    {
        CMenuIcon::AddIconToMenuItem(menu.GetSubMenu(0)->GetSafeHmenu(), 0, TRUE, GetMenuIcon(IDI_CONNECTION));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_DISPLAY_SETTINGS, FALSE, GetMenuIcon(IDI_ITEM));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_OPEN_TASK_MANAGER, FALSE, GetMenuIcon(IDI_TASK_MANAGER));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_OPTIONS2, FALSE, GetMenuIcon(IDI_SETTINGS));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_PLUGIN_MANAGE, FALSE, GetMenuIcon(IDI_PLUGINS));
        CMenuIcon::AddIconToMenuItem(menu.GetSubMenu(0)->GetSafeHmenu(), 8, TRUE, GetMenuIcon(IDI_HELP));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_HELP, FALSE, GetMenuIcon(IDI_HELP));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_APP_ABOUT, FALSE, GetMenuIcon(IDR_MAINFRAME));
        CMenuIcon::AddIconToMenuItem(menu.GetSafeHmenu(), ID_APP_EXIT, FALSE, GetMenuIcon(IDI_EXIT));
    };
    addIconsForTaksbarWindowMenu(m_taskbar_menu);
    addIconsForTaksbarWindowMenu(m_taskbar_menu_plugin);
}

HICON CTrafficMonitorApp::GetMenuIcon(UINT id)
{
    auto iter = m_menu_icons.find(id);
    if (iter != m_menu_icons.end())
    {
        return iter->second;
    }
    else
    {
        HICON hIcon = CCommon::LoadIconResource(id, DPI(16));
        m_menu_icons[id] = hIcon;
        return hIcon;
    }
}

void CTrafficMonitorApp::AutoSelectNotifyIcon()
{
    if (m_win_version.GetMajorVersion() >= 10)
    {
        bool light_mode = CWindowsSettingHelper::IsWindows10LightTheme();
        if (light_mode)     //浅色模式下，如果图标是白色，则改成黑色
        {
            if (m_cfg_data.m_notify_icon_selected == 0)
                m_cfg_data.m_notify_icon_selected = 4;
            if (m_cfg_data.m_notify_icon_selected == 1)
                m_cfg_data.m_notify_icon_selected = 5;
        }
        else     //深色模式下，如果图标是黑色，则改成白色
        {
            if (m_cfg_data.m_notify_icon_selected == 4)
                m_cfg_data.m_notify_icon_selected = 0;
            if (m_cfg_data.m_notify_icon_selected == 5)
                m_cfg_data.m_notify_icon_selected = 1;
        }
    }
}

// 唯一的一个 CTrafficMonitorApp 对象

CTrafficMonitorApp theApp;


// CTrafficMonitorApp 初始化

BOOL CTrafficMonitorApp::InitInstance()
{
    //替换掉对话框程序的默认类名
    WNDCLASS wc;
    ::GetClassInfo(AfxGetInstanceHandle(), _T("#32770"), &wc);       //MFC默认的所有对话框的类名为#32770
    wc.lpszClassName = APP_CLASS_NAME;      //将对话框的类名修改为新类名
    AfxRegisterClass(&wc);

    //设置配置文件的路径
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    m_module_path = path;
    if (m_module_path.find(L' ') != wstring::npos)
    {
        //如果路径中有空格，则在程序路径前后添加双引号
        m_module_path_reg = L'\"';
        m_module_path_reg += m_module_path;
        m_module_path_reg += L'\"';
    }
    else
    {
        m_module_path_reg = m_module_path;
    }
    m_module_dir = CCommon::GetModuleDir();
    m_system_dir = CCommon::GetSystemDir();
    m_appdata_dir = m_module_dir; // Compatibility field; never access AppData.

    LoadGlobalConfig();

    m_config_dir = m_module_dir;
    m_config_path = m_config_dir + L"config.ini";
    m_log_path = m_config_dir + L"error.log";

    //界面语言固定为英文
    CCommon::SetThreadLanguage(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US));

    //初始化字符串资源
    m_str_table.Init();

    //检查是否已有实例正在运行
    CIniHelper ini{ m_config_path };
    m_no_multistart_warning = ini.GetBool(_T("other"), _T("no_multistart_warning"), false);
    LPCTSTR mutex_name{};
#ifdef _DEBUG
    mutex_name = _T("SimpleTaskbarMonitor-e8Ahk24HP6JC8hDy");
#else
    mutex_name = _T("SimpleTaskbarMonitor-1419J3XLKL1w8OZc");
#endif
    HANDLE hMutex = ::CreateMutex(NULL, TRUE, mutex_name);
    if (hMutex != NULL)
    {
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            //char buff[128];
            //string cmd_line_str{ CCommon::UnicodeToStr(cmd_line.c_str()) };
            //sprintf_s(buff, "when_start=%d, m_no_multistart_warning=%d, cmd_line=%s", when_start, m_no_multistart_warning, cmd_line_str.c_str());
            //CCommon::WriteLog(buff, _T(".\\start.log"));
            if (!m_no_multistart_warning)
            {
                //查找已存在TrafficMonitor进程的主窗口的句柄
                HWND exist_handel = ::FindWindow(APP_CLASS_NAME, NULL);
                if (exist_handel != NULL)
                {
                    //弹出“TrafficMonitor已在运行”对话框
                    CAppAlreadyRuningDlg dlg(exist_handel);
                    dlg.DoModal();
                }
                else
                {
                    AfxMessageBox(CCommon::LoadText(IDS_AN_INSTANCE_RUNNING));
                }
            }
            return FALSE;
        }
    }

    //初始化GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

    //初始时使用屏幕DPI
    DPIFromWindow(nullptr);

    //载入插件
    LoadPluginDisabledSettings();
    m_plugins.LoadPlugins();

    //从ini文件载入设置
    LoadConfig();

    m_taskbar_default_style.LoadConfig();

    //SaveConfig();

    // 如果一个运行在 Windows XP 上的应用程序清单指定要
    // 使用 ComCtl32.dll 版本 6 或更高版本来启用可视化方式，
    //则需要 InitCommonControlsEx()。  否则，将无法创建窗口。
    INITCOMMONCONTROLSEX InitCtrls;
    InitCtrls.dwSize = sizeof(InitCtrls);
    // 将它设置为包括所有要在应用程序中使用的
    // 公共控件类。
    InitCtrls.dwICC = ICC_WIN95_CLASSES;
    InitCommonControlsEx(&InitCtrls);

    CWinApp::InitInstance();


    AfxEnableControlContainer();

    // 创建 shell 管理器，以防对话框包含
    // 任何 shell 树视图控件或 shell 列表视图控件。
    CShellManager* pShellManager = new CShellManager;

    // 激活“Windows Native”视觉管理器，以便在 MFC 控件中启用主题
    CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

    // 标准初始化
    // 如果未使用这些功能并希望减小
    // 最终可执行文件的大小，则应移除下列
    // 不需要的特定初始化例程
    // 更改用于存储设置的注册表项
    // TODO: 应适当修改该字符串，
    // 例如修改为公司或组织名
    //SetRegistryKey(_T("应用程序向导生成的本地应用程序"));        //暂不使用注册表保存数据


#ifndef WITHOUT_TEMPERATURE
    //检测是否安装.net framework 4.5
    if (!CWindowsSettingHelper::IsDotNetFramework4Point5Installed())
    {
        if (theApp.m_show_dot_net_notinstalled_tip)
        {
            if (AfxMessageBox(CCommon::LoadText(IDS_DOTNET_NOT_INSTALLED_TIP), MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL)       //点击“取消”不再提示
            {
                theApp.m_show_dot_net_notinstalled_tip = false;
                SaveConfig();
            }
        }
    }
    else
    {
        //如果没有开启任何一项的硬件监控，则不初始化OpenHardwareMonitor
        if (theApp.m_general_data.IsHardwareEnable(HI_CPU) || theApp.m_general_data.IsHardwareEnable(HI_GPU)
            || theApp.m_general_data.IsHardwareEnable(HI_HDD) || theApp.m_general_data.IsHardwareEnable(HI_MBD))
        {
            //启动初始化OpenHardwareMonitor的线程。由于OpenHardwareMonitor初始化需要一定的时间，为了防止启动时程序卡顿，将其放到后台线程中处理
            InitOpenHardwareLibInThread();
        }
    }
#endif


    SendSettingsToPlugin();

    //创建任务栏核心窗口。该窗口不可见，仅作为应用程序的隐藏主窗口，负责监控数据采集、
    //通知区图标以及任务栏窗口的生命周期。
    CTaskbarCore* pTaskbarCore = new CTaskbarCore();
    m_pMainWnd = pTaskbarCore;
    if (!pTaskbarCore->Create(IDD_TASKBAR_CORE_DIALOG))
    {
        TRACE(traceAppMsg, 0, "警告: 任务栏核心窗口创建失败，应用程序将意外终止。\n");
        delete pTaskbarCore;
        m_pMainWnd = nullptr;
        return FALSE;
    }
    pTaskbarCore->ShowWindow(SW_HIDE);

    // 删除上面创建的 shell 管理器。
    if (pShellManager != NULL)
    {
        delete pShellManager;
    }

#ifndef _AFXDLL
    ControlBarCleanUp();
#endif

    // 返回 TRUE 以启动应用程序的消息循环。应用程序的退出由任务栏核心窗口的关闭操作触发。
    return TRUE;
}

void CTrafficMonitorApp::InitOpenHardwareLibInThread()
{
#ifndef WITHOUT_TEMPERATURE
    AfxBeginThread(InitOpenHardwareMonitorLibThreadFunc, NULL);
#endif
}


void CTrafficMonitorApp::UpdateOpenHardwareMonitorEnableState()
{
#ifndef WITHOUT_TEMPERATURE
    if (m_pMonitor != nullptr)
    {
        CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
        m_pMonitor->SetCpuEnable(m_general_data.IsHardwareEnable(HI_CPU));
        m_pMonitor->SetGpuEnable(m_general_data.IsHardwareEnable(HI_GPU));
        m_pMonitor->SetHddEnable(m_general_data.IsHardwareEnable(HI_HDD));
        m_pMonitor->SetMainboardEnable(m_general_data.IsHardwareEnable(HI_MBD));
    }
#endif
}

//void CTrafficMonitorApp::UpdateTaskbarWndMenu()
//{
//    //获取“显示设置”子菜单
//    CMenu* pMenu = m_taskbar_menu.GetSubMenu(0)->GetSubMenu(5);
//    ASSERT(pMenu != nullptr);
//    if (pMenu != nullptr)
//    {
//        //将ID_SHOW_MEMORY_USAGE后面的所有菜单项删除
//        if (pMenu->GetMenuItemCount() > 4)
//        {
//            int start_pos = CCommon::GetMenuItemPosition(pMenu, ID_SHOW_MEMORY_USAGE) + 1;
//            while (pMenu->GetMenuItemCount() > start_pos)
//            {
//                pMenu->DeleteMenu(start_pos, MF_BYPOSITION);
//            }
//        }
//
//        //添加温度相关菜单项
//#ifndef WITHOUT_TEMPERATURE
//        if (m_general_data.IsHardwareEnable(HI_GPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_GPU, CCommon::LoadText(IDS_GPU_USAGE));
//        if (m_general_data.IsHardwareEnable(HI_CPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_CPU_TEMPERATURE, CCommon::LoadText(IDS_CPU_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_GPU))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_GPU_TEMPERATURE, CCommon::LoadText(IDS_GPU_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_HDD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_HDD_TEMPERATURE, CCommon::LoadText(IDS_HDD_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_MBD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_MAIN_BOARD_TEMPERATURE, CCommon::LoadText(IDS_MAINBOARD_TEMPERATURE));
//        if (m_general_data.IsHardwareEnable(HI_HDD))
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_HDD, CCommon::LoadText(IDS_HDD_USAGE));
//#endif
//
//        pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_TOTAL_SPEED, CCommon::LoadText(IDS_TOTAL_NET_SPEED));
//
//        //添加插件菜单项
//        if (!m_plugins.GetPluginItems().empty())
//            pMenu->AppendMenu(MF_SEPARATOR);
//        int plugin_index = 0;
//        for (const auto& plugin_item : m_plugins.GetPluginItems())
//        {
//            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SHOW_PLUGIN_ITEM_START + plugin_index, plugin_item->GetItemName());
//            plugin_index++;
//        }
//    }
//}

bool CTrafficMonitorApp::IsForceShowNotifyIcon()
{
    auto core = CTaskbarCore::Instance();
    return core == nullptr || !core->IsTaskbarWndValid();
}

std::wstring CTrafficMonitorApp::GetPlauginTooltipInfo() const
{
    std::wstring tip_info;
    for (const auto& item : m_plugins.GetPlugins())
    {
        if (item.plugin != nullptr && item.plugin->GetAPIVersion() >= 2)
        {
            std::wstring plugin_tooltip = item.plugin->GetTooltipInfo();
            CCommon::StringNormalize(plugin_tooltip);
            if (!plugin_tooltip.empty())
            {
                tip_info += L"\r\n";
                tip_info += plugin_tooltip.c_str();
            }
        }
    }
    return tip_info;
}

bool CTrafficMonitorApp::IsTaksbarItemDisplayed(CommonDisplayItem item) const
{
    if (item.IsPlugin())
    {
        if (item.PluginItem() != nullptr)
            return m_taskbar_data.plugin_display_item.Contains(item.PluginItem()->GetItemId());
    }
    else
    {
        return m_taskbar_data.display_item.Contains(item.ItemType());
    }
    return false;
}

void CTrafficMonitorApp::SendSettingsToPlugin()
{
    for (const auto& plugin_info : m_plugins.GetPlugins())
    {
        if (plugin_info.plugin != nullptr && plugin_info.plugin->GetAPIVersion() >= 2)
        {
            //主窗口已被裁剪，这里将任务栏窗口的设置发送给插件，以保持插件接口的兼容性
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_NAIN_WND_NET_SPEED_SHORT_MODE, std::to_wstring(m_taskbar_data.speed_short_mode).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_SPERATE_WITH_SPACE, std::to_wstring(m_taskbar_data.separate_value_unit_with_space).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_UNIT_BYTE, std::to_wstring(m_taskbar_data.unit_byte).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_UNIT_SELECT, std::to_wstring(static_cast<int>(m_taskbar_data.speed_unit)).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_NOT_SHOW_UNIT, std::to_wstring(m_taskbar_data.hide_unit).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_MAIN_WND_NOT_SHOW_PERCENT, std::to_wstring(m_taskbar_data.hide_percent).c_str());

            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NET_SPEED_SHORT_MODE, std::to_wstring(m_taskbar_data.speed_short_mode).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_SPERATE_WITH_SPACE, std::to_wstring(m_taskbar_data.separate_value_unit_with_space).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_VALUE_RIGHT_ALIGN, std::to_wstring(m_taskbar_data.value_right_align).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NET_SPEED_WIDTH, std::to_wstring(m_taskbar_data.digits_number).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_UNIT_BYTE, std::to_wstring(m_taskbar_data.unit_byte).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_UNIT_SELECT, std::to_wstring(static_cast<int>(m_taskbar_data.speed_unit)).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NOT_SHOW_UNIT, std::to_wstring(m_taskbar_data.hide_unit).c_str());
            plugin_info.plugin->OnExtenedInfo(ITMPlugin::EI_TASKBAR_WND_NOT_SHOW_PERCENT, std::to_wstring(m_taskbar_data.hide_percent).c_str());
        }
    }
}

void CTrafficMonitorApp::UpdatePluginMenu(CMenu* pMenu, ITMPlugin* plugin, int plugin_cmd_start_index)
{
    if (pMenu != nullptr)
    {
        //删除菜单已经存在的插件命令
        while (pMenu->GetMenuItemCount() > plugin_cmd_start_index)
        {
            if (!pMenu->DeleteMenu(pMenu->GetMenuItemCount() - 1, MF_BYPOSITION))
                break;
        }
        //添加插件命令
        if (plugin != nullptr && plugin->GetAPIVersion() >= 5)
        {
            int plugin_cmd_count = plugin->GetCommandCount();
            //添加分隔符
            if (plugin_cmd_count > 0)
                pMenu->AppendMenu(MF_SEPARATOR);
            for (int i = 0; i < plugin_cmd_count; i++)
            {
                const wchar_t* cmd_name = plugin->GetCommandName(i);
                if (cmd_name != nullptr)
                {
                    pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_PLUGIN_COMMAND_START + i, cmd_name);
                    //添加命令图标
                    HICON cmd_icon = (HICON)plugin->GetCommandIcon(i);
                    CMenuIcon::AddIconToMenuItem(pMenu->GetSafeHmenu(), ID_PLUGIN_COMMAND_START + i, FALSE, cmd_icon);
                }
            }
        }
    }
}

void CTrafficMonitorApp::CheckWindows11Taskbar()
{
    // 在“Shell_TrayWnd”的子窗口找到类名为“Windows.UI.Composition.DesktopWindowContentBridge”的窗口则认为是Windows11的任务栏
    if (m_win_version.IsWindows11OrLater())
    {
        HWND hTaskbar = ::FindWindow(L"Shell_TrayWnd", NULL);
        m_is_windows11_taskbar = (::FindWindowExW(hTaskbar, 0, L"Windows.UI.Composition.DesktopWindowContentBridge", NULL) != NULL);
    }
    else
    {
        m_is_windows11_taskbar = false;
    }
}

bool CTrafficMonitorApp::DPIFromRect(const RECT& rect, UINT* out_dpi_x, UINT* out_dpi_y)
{
    HMONITOR h_current_monitor = ::MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
    HRESULT hr = m_dll_functions.GetDpiForMonitor(h_current_monitor, MDT_EFFECTIVE_DPI, out_dpi_x, out_dpi_y);
    return hr == S_OK;
}

unsigned int CTrafficMonitorApp::GetThemeColor() const
{
    return m_theme_color;
}

void CTrafficMonitorApp::SetThemeColor(COLORREF color)
{
    m_theme_color = color;
}

void CTrafficMonitorApp::OnHelp()
{
    // TODO: 在此添加命令处理程序代码
    ShellExecute(NULL, _T("open"), _T("https://github.com/LinBlink/SimpleTaskbarMonitor/blob/master/README.md"), NULL, NULL, SW_SHOW);
}


void CTrafficMonitorApp::OnFrequentyAskedQuestions()
{
    // TODO: 在此添加命令处理程序代码
    ShellExecute(NULL, _T("open"), _T("https://github.com/LinBlink/SimpleTaskbarMonitor/blob/master/README.md"), NULL, NULL, SW_SHOW);
}


void CTrafficMonitorApp::OnUpdateLog()
{
    // TODO: 在此添加命令处理程序代码
    ShellExecute(NULL, _T("open"), _T("https://github.com/LinBlink/SimpleTaskbarMonitor/commits/master"), NULL, NULL, SW_SHOW);
}


int CTrafficMonitorApp::ExitInstance()
{
    // 释放GDI+
    Gdiplus::GdiplusShutdown(m_gdiplusToken);

    return CWinApp::ExitInstance();
}

int CTrafficMonitorApp::GetAPIVersion()
{
    return 1;
}

const wchar_t* CTrafficMonitorApp::GetVersion()
{
    return VERSION;
}

double CTrafficMonitorApp::GetMonitorValue(MonitorItem item)
{
    switch (item)
    {
    case MI_UP: return m_out_speed;
    case MI_DOWN: return m_in_speed;
    case MI_CPU: return m_cpu_usage;
    case MI_MEMORY: return m_memory_usage;
    case MI_GPU_USAGE: return m_gpu_usage;
    case MI_CPU_TEMP: return m_cpu_temperature;
    case MI_GPU_TEMP: return m_gpu_temperature;
    case MI_HDD_TEMP: return m_hdd_temperature;
    case MI_MAIN_BOARD_TEMP: return m_main_board_temperature;
    case MI_HDD_USAGE: return m_hdd_usage;
    case MI_CPU_FREQ: return m_cpu_freq;
    case MI_TODAY_UP_TRAFFIC: return m_today_up_traffic;
    case MI_TODAY_DOWN_TRAFFIC: return m_today_down_traffic;
    }
    return 0.0;
}

const wchar_t* CTrafficMonitorApp::GetMonitorValueString(MonitorItem item, int is_main_window)
{
    static CString str_value;
    if (item == MI_TODAY_UP_TRAFFIC)
    {
        str_value = CCommon::KBytesToString(theApp.m_today_up_traffic / 1024u);
    }
    else if (item == MI_TODAY_DOWN_TRAFFIC)
    {
        str_value = CCommon::KBytesToString(theApp.m_today_down_traffic / 1024u);
    }
    else
    {
        DisplayItem display_item{};
        switch (item)
        {
        case MI_UP: display_item = DisplayItem::TDI_UP; break;
        case MI_DOWN: display_item = DisplayItem::TDI_DOWN; break;
        case MI_CPU: display_item = DisplayItem::TDI_CPU; break;
        case MI_MEMORY: display_item = DisplayItem::TDI_MEMORY; break;
        case MI_GPU_USAGE: display_item = DisplayItem::TDI_GPU_USAGE; break;
        case MI_CPU_TEMP: display_item = DisplayItem::TDI_CPU_TEMP; break;
        case MI_GPU_TEMP: display_item = DisplayItem::TDI_GPU_TEMP; break;
        case MI_HDD_TEMP: display_item = DisplayItem::TDI_HDD_TEMP; break;
        case MI_MAIN_BOARD_TEMP: display_item = DisplayItem::TDI_MAIN_BOARD_TEMP; break;
        case MI_HDD_USAGE: display_item = DisplayItem::TDI_HDD_USAGE; break;
        case MI_CPU_FREQ: display_item = DisplayItem::TDI_CPU_FREQ; break;
        }
        str_value = CommonDisplayItem(display_item).GetItemValueText(false);
    }
    return str_value.GetString();
}

void CTrafficMonitorApp::ShowNotifyMessage(const wchar_t* strMsg)
{
    CTaskbarCore* pMainWnd = dynamic_cast<CTaskbarCore*>(m_pMainWnd);
    if (pMainWnd != nullptr)
    {
        pMainWnd->ShowNotifyTip(CCommon::LoadText(IDS_TRAFFICMONITOR_PLUGIN_NITIFICATION), strMsg);
    }
}

unsigned short CTrafficMonitorApp::GetLanguageId() const
{
    return MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
}

const wchar_t* CTrafficMonitorApp::GetPluginConfigDir() const
{
    static std::wstring config_dir;
    config_dir = m_config_dir;
    config_dir += L"plugins\\";
    return config_dir.c_str();
}

int CTrafficMonitorApp::GetDPI(DPIType type) const
{
    CTaskbarCore* pMainWnd = CTaskbarCore::Instance();
    switch (type)
    {
    case DPI_MAIN_WND:
        return m_dpi;
    case DPI_TASKBAR:
        if (pMainWnd != nullptr)
        {
            if (pMainWnd->IsTaskbarWndValid())
                return pMainWnd->GetTaskbarWindow()->GetDPI();
        }
        return m_dpi;
    }
    return 0;
}

void* CTrafficMonitorApp::GetMainWindowHwnd()
{
    return m_pMainWnd->GetSafeHwnd();
}

void* CTrafficMonitorApp::GetTaskbarWindowHwnd()
{
    return CTaskbarCore::Instance()->GetTaskbarWindow()->GetSafeHwnd();
}

const wchar_t* CTrafficMonitorApp::GetStringRes(const wchar_t* key, const wchar_t* section)
{
    return m_str_table.LoadText(key, section).c_str();
}
