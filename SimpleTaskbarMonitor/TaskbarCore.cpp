// TaskbarCore.cpp : 实现文件
//

#include "stdafx.h"
#include "TrafficMonitor.h"
#include "TaskbarCore.h"
#include "afxdialogex.h"
#include "PluginManagerDlg.h"
#include "SetItemOrderDlg.h"
#include "WindowsSettingHelper.h"
#include "PluginInfoDlg.h"
#include "SupportedRenderEnums.h"
#include "ClassicalTaskbarDlg.h"
#include "Win11TaskbarDlg.h"
#include "WineTaskbarDlg.h"
#include "TaskbarHelper.h"
#include "Builtin/BuiltinFeatures.h"
#include "Builtin/Temperature.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif



// CTaskbarCore —— 任务栏核心
//
// 本程序已裁剪为只保留“在任务栏上显示信息”的功能，CTaskbarCore 不再对应一个可见的主窗口，
// 它作为应用程序的隐藏主窗口，负责：
//   · 监控数据的采集（网速、CPU/内存/硬盘/温度）
//   · 网络连接的选择与自动选择
//   · 通知区（托盘）图标
//   · 任务栏窗口的创建、关闭与参数刷新

//静态成员初始化
unsigned int CTaskbarCore::m_WM_TASKBARCREATED{ ::RegisterWindowMessage(_T("TaskbarCreated")) };  //注册任务栏建立的消息

CTaskbarCore::CTaskbarCore(CWnd* pParent /*=NULL*/)
    : CDialog(IDD_TASKBAR_CORE_DIALOG, pParent)
{
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    m_desktop_dc = ::GetDC(NULL);
}

CTaskbarCore::~CTaskbarCore()
{

    if (m_tBarDlg != nullptr)
    {
        delete m_tBarDlg;
        m_tBarDlg = nullptr;
    }

    ::ReleaseDC(NULL, m_desktop_dc);

    // Destroy notify area icon handles loaded via LoadImage in OnInitDialog
    // LoadImage creates a copy of the icon resource; each must be freed with DestroyIcon
    for (int i = 0; i < MAX_NOTIFY_ICON; i++)
    {
        if (theApp.m_notify_icons[i] != NULL)
        {
            ::DestroyIcon(theApp.m_notify_icons[i]);
            theApp.m_notify_icons[i] = NULL;
        }
    }
}

CTaskBarDlg* CTaskbarCore::GetTaskbarWindow() const
{
    if (IsTaskbarWndValid())
        return m_tBarDlg;
    else
        return nullptr;
}

CTaskbarCore* CTaskbarCore::Instance()
{
    return dynamic_cast<CTaskbarCore*>(theApp.m_pMainWnd);
}

void CTaskbarCore::DoDataExchange(CDataExchange* pDX)
{
    CDialog::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CTaskbarCore, CDialog)
    ON_WM_TIMER()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
    ON_MESSAGE(MY_WM_NOTIFYICON, &CTaskbarCore::OnNotifyIcon)
    ON_COMMAND(ID_SHOW_NOTIFY_ICON, &CTaskbarCore::OnShowNotifyIcon)
    ON_COMMAND(ID_APP_ABOUT, &CTaskbarCore::OnAppAbout)
    ON_COMMAND(ID_SHOW_CPU_MEMORY2, &CTaskbarCore::OnShowCpuMemory2)
    ON_REGISTERED_MESSAGE(m_WM_TASKBARCREATED, &CTaskbarCore::OnTaskBarCreated)
    ON_COMMAND(ID_OPTIONS, &CTaskbarCore::OnOptions)
    ON_COMMAND(ID_OPTIONS2, &CTaskbarCore::OnOptions2)
    ON_MESSAGE(WM_EXITMENULOOP, &CTaskbarCore::OnExitmenuloop)
    ON_COMMAND(ID_CHANGE_NOTIFY_ICON, &CTaskbarCore::OnChangeNotifyIcon)
    ON_MESSAGE(WM_TASKBAR_MENU_POPED_UP, &CTaskbarCore::OnTaskbarMenuPopedUp)
    ON_COMMAND(ID_SHOW_NET_SPEED, &CTaskbarCore::OnShowNetSpeed)
    ON_WM_QUERYENDSESSION()
    ON_MESSAGE(WM_DPICHANGED, &CTaskbarCore::OnDpichanged)
    ON_MESSAGE(WM_MONITOR_INFO_UPDATED, &CTaskbarCore::OnMonitorInfoUpdated)
    ON_MESSAGE(WM_DISPLAYCHANGE, &CTaskbarCore::OnDisplaychange)
    ON_COMMAND(ID_PLUGIN_MANAGE, &CTaskbarCore::OnPluginManage)
    ON_MESSAGE(WM_REOPEN_TASKBAR_WND, &CTaskbarCore::OnReopenTaksbarWnd)
    ON_COMMAND(ID_OPEN_TASK_MANAGER, &CTaskbarCore::OnOpenTaskManager)
    ON_MESSAGE(WM_SETTINGS_APPLIED, &CTaskbarCore::OnSettingsApplied)
    ON_COMMAND(ID_DISPLAY_SETTINGS, &CTaskbarCore::OnDisplaySettings)
    ON_COMMAND(ID_REFRESH_CONNECTION_LIST, &CTaskbarCore::OnRefreshConnectionList)
    ON_MESSAGE(WM_TABLET_QUERYSYSTEMGESTURESTATUS, &CTaskbarCore::OnTabletQuerysystemgesturestatus)
    ON_COMMAND(ID_PLUGIN_OPTIONS_TASKBAR, &CTaskbarCore::OnPluginOptionsTaksbar)
    ON_COMMAND(ID_PLUGIN_DETAIL_TASKBAR, &CTaskbarCore::OnPluginDetailTaksbar)
    ON_WM_POWERBROADCAST()
    ON_WM_DWMCOLORIZATIONCOLORCHANGED()
END_MESSAGE_MAP()




void CTaskbarCore::GetScreenSize()
{
    m_screen_size.cx = GetSystemMetrics(SM_CXSCREEN);
    m_screen_size.cy = GetSystemMetrics(SM_CYSCREEN);
}


void CTaskbarCore::AutoSelect()
{
    unsigned __int64 max_in_out_bytes{};
    unsigned __int64 in_out_bytes;
    //m_connection_selected = m_connections[0].index;
    m_connection_selected = 0;
    //自动选择连接时，查找已发送和已接收字节数之和最多的那个连接，并将其设置为当前查看的连接
    for (size_t i{}; i < m_connections.size(); i++)
    {
        auto table = GetConnectIfTable(i);
        if (table.dwOperStatus == IF_OPER_STATUS_OPERATIONAL)     //只选择网络状态为正常的连接
        {
            in_out_bytes = table.dwInOctets + table.dwOutOctets;
            if (in_out_bytes > max_in_out_bytes)
            {
                max_in_out_bytes = in_out_bytes;
                m_connection_selected = i;
            }
        }
    }
    theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;
    m_connection_change_flag = true;
}

void CTaskbarCore::IniConnection(bool refresh)
{
    if (refresh)
        m_if_table.Refresh();
    m_pIfTable = m_if_table.Get();

    //获取当前所有的连接，并保存到m_connections容器中
    if (!theApp.m_general_data.show_all_interface)
    {
        m_connections.clear();
        vector<NetWorkConection> connections;
        CAdapterCommon::GetAdapterInfo(connections);
        for (const auto& item : connections)
        {
            if (!theApp.m_general_data.connections_hide.Contains(CCommon::StrToUnicode(item.description.c_str())))
                m_connections.push_back(item);
        }
        CAdapterCommon::GetIfTableInfo(m_connections, m_pIfTable);
    }
    else
    {
        CAdapterCommon::GetAllIfTableInfo(m_connections, m_pIfTable);
    }

    //如果在设置了“显示所有网络连接”时设置了“选择全部”，则改为“自动选择”
    if (theApp.m_general_data.show_all_interface && theApp.m_cfg_data.m_select_all)
    {
        theApp.m_cfg_data.m_select_all = false;
        theApp.m_cfg_data.m_auto_select = true;
    }

    //写入调试日志
    if (theApp.m_debug_log)
    {
        CString log_str;
        log_str += _T("正在初始化网络连接...\n");
        log_str += _T("连接列表：\n");
        for (size_t i{}; i < m_connections.size(); i++)
        {
            log_str += m_connections[i].description.c_str();
            log_str += _T(", ");
            log_str += CCommon::IntToString(m_connections[i].index);
            log_str += _T("\n");
        }
        log_str += _T("IfTable:\n");
        for (size_t i{}; i < m_pIfTable->dwNumEntries; i++)
        {
            log_str += CCommon::IntToString(i);
            log_str += _T(" ");
            log_str += (const char*)m_pIfTable->table[i].bDescr;
            log_str += _T("\n");
        }
        CCommon::WriteLog(log_str, (theApp.m_config_dir + L".\\connections.log").c_str());
    }

    //if (m_connection_selected < 0 || m_connection_selected >= m_connections.size() || m_auto_select)
    //  AutoSelect();
    //选择网络连接
    if (theApp.m_cfg_data.m_auto_select)    //自动选择
    {
        if (m_restart_cnt != -1)    //当m_restart_cnt不等于-1时，即不是第一次初始化时，需要延时5秒再重新初始化连接
        {
            KillTimer(DELAY_TIMER);
            SetTimer(DELAY_TIMER, 5000, NULL);
        }
        else
        {
            AutoSelect();
        }
    }
    else        //查找网络名为上次选择的连接
    {
        m_connection_selected = 0;
        for (size_t i{}; i < m_connections.size(); i++)
        {
            if (m_connections[i].description_2 == m_connection_name_preferd)
                m_connection_selected = i;
        }
    }
    if (m_connection_selected < 0 || m_connection_selected >= m_connections.size())
        m_connection_selected = 0;
    theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;

    //根据已获取到的连接在菜单中添加相应项目
    IniTaskBarConnectionMenu();     //初始化任务栏窗口中的“选择网络连接”子菜单项

    IniTaskBarConnectionMenu();     //初始化任务栏窗口中的“选择网络连接”子菜单项

    m_restart_cnt++;    //记录初始化次数
    m_connection_change_flag = true;
}

MIB_IFROW CTaskbarCore::GetConnectIfTable(int connection_index)
{
    if (connection_index >= 0 && connection_index < static_cast<int>(m_connections.size()))
    {
        int index = m_connections[connection_index].index;
        if (m_pIfTable != nullptr && index >= 0 && index < m_pIfTable->dwNumEntries)
            return m_pIfTable->table[index];
    }
    return MIB_IFROW();
}

NetWorkConection CTaskbarCore::GetConnection(int connection_index)
{
    if (connection_index >= 0 && connection_index < static_cast<int>(m_connections.size()))
        return m_connections[connection_index];
    else
        return NetWorkConection();
}

void CTaskbarCore::IniConnectionMenu(CMenu* pMenu)
{
    ASSERT(pMenu != nullptr);
    if (pMenu != nullptr)
    {
        //先将ID_SELECT_ALL_CONNECTION后面的所有菜单项删除
        int start_pos = CCommon::GetMenuItemPosition(pMenu, ID_SELECT_ALL_CONNECTION) + 1;
        while (pMenu->GetMenuItemCount() > start_pos)
        {
            pMenu->DeleteMenu(start_pos, MF_BYPOSITION);
        }

        CString connection_descr;
        for (size_t i{}; i < m_connections.size(); i++)
        {
            connection_descr = CCommon::StrToUnicode(m_connections[i].description.c_str()).c_str();
            pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_SELECT_ALL_CONNECTION + i + 1, connection_descr);
        }

        //添加“刷新网络列表”命令
        pMenu->AppendMenu(MF_SEPARATOR);
        pMenu->AppendMenu(MF_STRING | MF_ENABLED, ID_REFRESH_CONNECTION_LIST, CCommon::LoadText(IDS_REFRESH_CONNECTION_LIST));
    }
}

void CTaskbarCore::IniTaskBarConnectionMenu()
{
    //向“选择网络连接”子菜单项添加项目
    IniConnectionMenu(theApp.m_taskbar_menu.GetSubMenu(0)->GetSubMenu(0));
    IniConnectionMenu(theApp.m_taskbar_menu_plugin.GetSubMenu(0)->GetSubMenu(0));
}

void CTaskbarCore::SetConnectionMenuState(CMenu* pMenu)
{
    if (theApp.m_cfg_data.m_select_all)
        pMenu->CheckMenuRadioItem(0, m_connections.size() + 1, 1, MF_BYPOSITION | MF_CHECKED);
    else if (theApp.m_cfg_data.m_auto_select)       //m_auto_select为true时为自动选择，选中菜单的第1项
        pMenu->CheckMenuRadioItem(0, m_connections.size() + 1, 0, MF_BYPOSITION | MF_CHECKED);
    else        //m_auto_select为false时非自动选择，根据m_connection_selected的值选择对应的项
        pMenu->CheckMenuRadioItem(0, m_connections.size() + 1, m_connection_selected + 2, MF_BYPOSITION | MF_CHECKED);

    //没有设置为“选择全部”时，将当前选择项设置为默认菜单项（加粗显示）
    if (!theApp.m_cfg_data.m_select_all)
        pMenu->SetDefaultItem(m_connection_selected + 2, TRUE);
    else
        pMenu->SetDefaultItem(-1, TRUE);
}

void CTaskbarCore::CloseTaskBarWnd()
{
    if (m_tBarDlg != nullptr)
    {
        if (IsTaskbarWndValid())
            m_tBarDlg->OnCancel();
        delete m_tBarDlg;
        m_tBarDlg = nullptr;
        theApp.m_taskbar_data.update_layered_window_error_code = 0;
    }
}

void CTaskbarCore::OpenTaskBarWnd()
{
    // 强制初始化theApp.m_is_windows11_taskbar的值
    theApp.CheckWindows11Taskbar();
    if (theApp.m_win_version.IsWine())
        m_tBarDlg = new CWineTaskbarDlg();
    else if (theApp.IsWindows11Taskbar())
        m_tBarDlg = new CWin11TaskbarDlg();
    else
        m_tBarDlg = new CClassicalTaskbarDlg();

    CSupportedRenderEnums supported_render_enums{};
    CTaskBarDlg::DisableRenderFeatureIfNecessary(supported_render_enums);
    auto render_type = supported_render_enums.GetAutoFitEnum();
    // WS_EX_LAYERED 和 WS_EX_NOREDIRECTIONBITMAP 可以共存，见微软示例代码
    // https://github.com/microsoft/Windows-classic-samples/blob/7cbd99ac1d2b4a0beffbaba29ea63d024ceff700/Samples/DynamicDPI/cpp/SampleDesktopWindow.cpp#L179
    // 但是WS_EX_NOREDIRECTIONBITMAP似乎会导致UpdateLayeredWindowIndirect失败
    switch (render_type)
    {
        using namespace DrawCommonHelper;
    case RenderType::D2D1_WITH_DCOMPOSITION:
        m_tBarDlg->Create(IDD_TASK_BAR_DIALOG_NOREDIRECTIONBITMAP, this);
        break;
    // 包括RenderType::D2D1在内的其他值
    default:
        m_tBarDlg->Create(IDD_TASK_BAR_DIALOG, this);
        break;
    }
    m_tBarDlg->ShowWindow(SW_SHOW);
    //m_tBarDlg->ShowInfo();
    //IniTaskBarConnectionMenu();
}

void CTaskbarCore::AddNotifyIcon()
{
    const bool reopen = IsTaskbarWndValid();
    if (reopen)
        CloseTaskBarWnd();
    ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon);
    if (reopen)
        OpenTaskBarWnd();
}

void CTaskbarCore::DeleteNotifyIcon()
{
    const bool reopen = IsTaskbarWndValid();
    if (reopen)
        CloseTaskBarWnd();
    ::Shell_NotifyIcon(NIM_DELETE, &m_ntIcon);
    if (reopen)
        OpenTaskBarWnd();
}

void CTaskbarCore::ShowNotifyTip(const wchar_t* title, const wchar_t* message)
{
    //要显示通知区提示，必须先将通知区图标显示出来
    if (!theApp.m_general_data.show_notify_icon)
    {
        //添加通知栏图标
        AddNotifyIcon();
    }
    //显示通知提示
    m_ntIcon.uFlags |= NIF_INFO;
    //wcscpy_s(m_ntIcon.szInfo, message ? message : _T(""));
    //wcscpy_s(m_ntIcon.szInfoTitle, title ? title : _T(""));
    CCommon::WStringCopy(m_ntIcon.szInfo, 256, message);
    CCommon::WStringCopy(m_ntIcon.szInfoTitle, 64, title);
    ::Shell_NotifyIcon(NIM_MODIFY, &m_ntIcon);
    m_ntIcon.uFlags &= ~NIF_INFO;

    //如果不显示通知区域图标，则在弹出通知的一段时间后删除通知区图标
    if (!theApp.m_general_data.show_notify_icon)
    {
        //延迟一定时间后删除通知区图标
        KillTimer(DELETE_NOTIFY_ICON_TIMER);
        SetTimer(DELETE_NOTIFY_ICON_TIMER, 8000, NULL);
    }
}

void CTaskbarCore::UpdateNotifyIconTip()
{
    CString strTip;         //鼠标指向图标时显示的提示
#ifdef _DEBUG
    strTip = CCommon::LoadText(IDS_TRAFFICMONITOR, _T(" (Debug)"));
#else
    strTip = CCommon::LoadText(IDS_TRAFFICMONITOR);
#endif

    CString in_speed = CCommon::DataSizeToString(theApp.m_in_speed);
    CString out_speed = CCommon::DataSizeToString(theApp.m_out_speed);

    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%>/s"), { CCommon::LoadText(IDS_UPLOAD), out_speed });
    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%>/s"), { CCommon::LoadText(IDS_DOWNLOAD), in_speed });
    strTip += CCommon::StringFormat(_T("\r\nCPU: <%1%> %"), { theApp.m_cpu_usage });
    strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_MEMORY), theApp.m_memory_usage });
    if (IsTemperatureNeeded())
    {
        if (theApp.m_general_data.IsHardwareEnable(HI_GPU) && theApp.m_gpu_usage >= 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_GPU_USAGE), theApp.m_gpu_usage });
        if (theApp.m_general_data.IsHardwareEnable(HI_CPU) && theApp.m_cpu_temperature > 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_CPU_TEMPERATURE), static_cast<int>(theApp.m_cpu_temperature) });
        if (theApp.m_general_data.IsHardwareEnable(HI_GPU) && theApp.m_gpu_temperature > 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_GPU_TEMPERATURE), static_cast<int>(theApp.m_gpu_temperature) });
        if (theApp.m_general_data.IsHardwareEnable(HI_HDD) && theApp.m_hdd_temperature > 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_HDD_TEMPERATURE), static_cast<int>(theApp.m_hdd_temperature) });
        if (theApp.m_general_data.IsHardwareEnable(HI_MBD) && theApp.m_main_board_temperature > 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> °C"), { CCommon::LoadText(IDS_MAINBOARD_TEMPERATURE), static_cast<int>(theApp.m_main_board_temperature) });
        if (theApp.m_general_data.IsHardwareEnable(HI_HDD) && theApp.m_hdd_usage >= 0)
            strTip += CCommon::StringFormat(_T("\r\n<%1%>: <%2%> %"), { CCommon::LoadText(IDS_HDD_USAGE), theApp.m_hdd_usage });
    }

    CCommon::WStringCopy(m_ntIcon.szTip, 128, strTip);
    ::Shell_NotifyIcon(NIM_MODIFY, &m_ntIcon);

}


void CTaskbarCore::_OnOptions(int tab, CWnd* pParent)
{
    COptionsDlg optionsDlg(tab, pParent);

    //将选项设置数据传递给选项设置对话框
    if (COptionsDlg::GetUniqueHandel(OPTION_DLG_NAME) == NULL)     //确保此时选项设置对话框已经关闭
    {
        optionsDlg.m_tab1_dlg.m_data = theApp.m_taskbar_data;
        optionsDlg.m_tab2_dlg.m_data = theApp.m_general_data;
    }

    if (optionsDlg.DoModal() == IDOK)
    {
        ApplySettings(optionsDlg);
    }
}

void CTaskbarCore::ApplySettings(COptionsDlg& optionsDlg)
{
    bool is_hardware_monitor_item_changed = (optionsDlg.m_tab2_dlg.m_data.hardware_monitor_item != theApp.m_general_data.hardware_monitor_item);
    bool is_show_notify_icon_changed = (optionsDlg.m_tab2_dlg.m_data.show_notify_icon != theApp.m_general_data.show_notify_icon);
    bool is_connections_hide_changed = (optionsDlg.m_tab2_dlg.m_data.connections_hide.data() != theApp.m_general_data.connections_hide.data());
    bool d2d_turned_on = (theApp.m_taskbar_data.disable_d2d && !optionsDlg.m_tab1_dlg.m_data.disable_d2d);
    //需要重新关闭再打开任务栏窗口的情况
    bool taskbar_changed = (theApp.m_taskbar_data.show_taskbar_wnd_in_secondary_display != optionsDlg.m_tab1_dlg.m_data.show_taskbar_wnd_in_secondary_display
        || theApp.m_taskbar_data.secondary_display_index != optionsDlg.m_tab1_dlg.m_data.secondary_display_index
        || theApp.m_taskbar_data.disable_d2d != optionsDlg.m_tab1_dlg.m_data.disable_d2d
        || theApp.m_taskbar_data.IsTaskbarTransparent() != optionsDlg.m_tab1_dlg.m_data.IsTaskbarTransparent()
        || theApp.m_taskbar_data.auto_set_background_color != optionsDlg.m_tab1_dlg.m_data.auto_set_background_color
        );

    theApp.m_taskbar_data = optionsDlg.m_tab1_dlg.m_data;
    theApp.m_general_data = optionsDlg.m_tab2_dlg.m_data;
    theApp.SendSettingsToPlugin();

    CGeneralSettingsDlg::CheckTaskbarDisplayItem();

    //打开了D2D渲染后自动开启“背景透明”并关闭“根据任务栏颜色自动设置背景色”
    if (d2d_turned_on)
    {
        theApp.m_taskbar_data.SetTaskabrTransparent(true);
        theApp.m_taskbar_data.auto_set_background_color = false;
    }

    //CTaskBarDlg::SaveConfig();
    if (IsTaskbarWndValid())
    {
        m_tBarDlg->ApplySettings();
        //如果更改了任务栏窗口字体或显示的文本，则任务栏窗口可能要变化，于是关闭再打开任务栏窗口
        if (taskbar_changed)
        {
            CloseTaskBarWnd();
            OpenTaskBarWnd();
        }
        else
        {
            m_tBarDlg->WidthChanged();
        }
        m_tBarDlg->ApplyWindowTransparentColor();
    }

    if (optionsDlg.m_tab2_dlg.IsAutoRunModified())
    {
        if (!theApp.SetAutoRun(theApp.m_general_data.auto_run, theApp.m_general_data.auto_run_by_task_scheduler))
            MessageBox(CCommon::LoadText(IDS_SET_AUTO_RUN_FAILED_WARNING), NULL, MB_ICONWARNING | MB_OK);
    }

    if (optionsDlg.m_tab2_dlg.IsShowAllInterfaceModified() || is_connections_hide_changed)
        IniConnection();

    if (optionsDlg.m_tab2_dlg.IsMonitorTimeSpanModified())      //如果监控时间间隔改变了，则重设定时器
    {
        KillTimer(MONITOR_TIMER);
        SetTimer(MONITOR_TIMER, theApp.m_general_data.monitor_time_span, NULL);
    }

#ifndef WITHOUT_TEMPERATURE
    if (is_hardware_monitor_item_changed)
    {
        //如果关闭了硬件监控，则析构硬件监控类
        if (theApp.m_general_data.hardware_monitor_item == 0)
        {
            CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
            theApp.m_pMonitor.reset();
        }
        else if (theApp.m_pMonitor != nullptr)
        {
            theApp.UpdateOpenHardwareMonitorEnableState();
        }
        else if (IsTemperatureNeeded())
        {
            theApp.InitOpenHardwareLibInThread();
        }
        //更新任务栏窗口右键菜单
        //theApp.UpdateTaskbarWndMenu();
    }
#endif

    if (is_show_notify_icon_changed)
    {
        if (theApp.IsForceShowNotifyIcon())
            theApp.m_general_data.show_notify_icon = true;
        if (theApp.m_general_data.show_notify_icon)
            AddNotifyIcon();
        else
            DeleteNotifyIcon();
    }

    theApp.SaveConfig();
    theApp.SaveGlobalConfig();
}


bool CTaskbarCore::IsTaskbarWndValid() const
{
    return m_tBarDlg != nullptr && ::IsWindow(m_tBarDlg->GetSafeHwnd());
}

void CTaskbarCore::TaskbarShowHideItem(DisplayItem type)
{
    if (IsTaskbarWndValid())
    {
        bool show = (theApp.m_taskbar_data.display_item.Contains(type));
        if (show)
        {
            theApp.m_taskbar_data.display_item.Remove(type);
        }
        else
        {
            theApp.m_taskbar_data.display_item.Add(type);
        }
        //CloseTaskBarWnd();
        //OpenTaskBarWnd();
        m_tBarDlg->WidthChanged();
    }
}


bool CTaskbarCore::IsTemperatureNeeded() const
{
    return theApp.m_general_data.IsHardwareEnable(HI_CPU) || theApp.m_general_data.IsHardwareEnable(HI_GPU)
        || theApp.m_general_data.IsHardwareEnable(HI_HDD) || theApp.m_general_data.IsHardwareEnable(HI_MBD);
}

// CTaskbarCore 消息处理程序

BOOL CTaskbarCore::PreCreateWindow(CREATESTRUCT& cs)
{
    if (!CDialog::PreCreateWindow(cs))
        return FALSE;

    //核心窗口只是一个隐藏的消息窗口，不应该显示出来，也不应该出现在任务栏中
    cs.style &= ~WS_VISIBLE;
    cs.dwExStyle |= WS_EX_TOOLWINDOW;
    return TRUE;
}


BOOL CTaskbarCore::OnInitDialog()
{
    CDialog::OnInitDialog();

    //核心窗口始终保持隐藏
    ShowWindow(SW_HIDE);

    // 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
    //  执行此操作
    SetIcon(m_hIcon, TRUE);         // 设置大图标
    SetIcon(m_hIcon, FALSE);        // 设置小图标

    SetWindowText(APP_NAME);
    //设置隐藏任务栏图标
    ModifyStyleEx(WS_EX_APPWINDOW, WS_EX_TOOLWINDOW);

    theApp.DPIFromWindow(this);
    //获取屏幕大小
    GetScreenSize();

    //初始化菜单
    theApp.InitMenuResourse();

    IniConnection();    //初始化连接

    //载入通知区图标
    theApp.m_notify_icons[0] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_NOFITY_ICON), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    theApp.m_notify_icons[1] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_NOFITY_ICON2), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    theApp.m_notify_icons[2] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_NOFITY_ICON3), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    theApp.m_notify_icons[3] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDR_MAINFRAME), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    theApp.m_notify_icons[4] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_NOFITY_ICON4), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);
    theApp.m_notify_icons[5] = (HICON)LoadImage(AfxGetInstanceHandle(), MAKEINTRESOURCE(IDI_NOTIFY_ICON5), IMAGE_ICON, theApp.DPI(16), theApp.DPI(16), LR_DEFAULTCOLOR | LR_CREATEDIBSECTION);

    //设置通知区域图标
    m_ntIcon.cbSize = sizeof(NOTIFYICONDATA);   //该结构体变量的大小
    if (theApp.m_cfg_data.m_notify_icon_selected < 0 || theApp.m_cfg_data.m_notify_icon_selected >= MAX_NOTIFY_ICON)
        theApp.m_cfg_data.m_notify_icon_selected = 0;
    m_ntIcon.hIcon = theApp.m_notify_icons[theApp.m_cfg_data.m_notify_icon_selected];       //设置图标
    m_ntIcon.hWnd = this->m_hWnd;               //接收托盘图标通知消息的窗口句柄
    CString atip;           //鼠标指向图标时显示的提示
#ifdef _DEBUG
    atip = CCommon::LoadText(IDS_TRAFFICMONITOR, _T(" (Debug)"));
#else
    atip = CCommon::LoadText(IDS_TRAFFICMONITOR);
#endif
    CCommon::WStringCopy(m_ntIcon.szTip, 128, atip.GetString());
    m_ntIcon.uCallbackMessage = MY_WM_NOTIFYICON;   //应用程序定义的消息ID号
    m_ntIcon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; //图标的属性：设置成员uCallbackMessage、hIcon、szTip有效
    if (theApp.m_general_data.show_notify_icon)
        ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon); //在系统通知区域增加这个图标

    //设置1000毫秒触发的定时器
    SetTimer(MAIN_TIMER, 1000, NULL);

    SetTimer(MONITOR_TIMER, theApp.m_general_data.monitor_time_span, NULL);
    m_monitor_thread_started = (AfxBeginThread(MonitorThreadCallback, (LPVOID)this) != nullptr);

    //获取启动时的时间
    GetLocalTime(&m_start_time);

    SetTimer(TASKBAR_TIMER, 100, NULL);

    return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}



//计算指定秒数的时间内Monitor定时器会触发的次数
static int GetMonitorTimerCount(int second)
{
    int count = second * 1000 / theApp.m_general_data.monitor_time_span;
    if (count <= 0) count = 1;
    return count;
}


void CTaskbarCore::DoNetworkAcquisition()
{
    if (m_menu_popuped) return;
    const bool valid = m_if_table.Refresh();
    m_pIfTable = m_if_table.Get();
    if (m_if_table.Changed())
        IniConnection(false);
    if (!valid)
    {
        theApp.m_in_speed = theApp.m_out_speed = 0;
        m_last_in_bytes = m_last_out_bytes = 0;
        m_connection_change_flag = true;
        return;
    }

    if (!theApp.m_cfg_data.m_select_all)        //获取当前选中连接的网速
    {
        auto table = GetConnectIfTable(m_connection_selected);
        m_in_bytes = table.dwInOctets;
        m_out_bytes = table.dwOutOctets;
    }
    else        //获取全部连接的网速
    {
        m_in_bytes = 0;
        m_out_bytes = 0;
        for (size_t i{}; i < m_connections.size(); i++)
        {
            auto table = GetConnectIfTable(i);
            //if (i > 0 && m_pIfTable->table[m_connections[i].index].dwInOctets == m_pIfTable->table[m_connections[i - 1].index].dwInOctets
            //  && m_pIfTable->table[m_connections[i].index].dwOutOctets == m_pIfTable->table[m_connections[i - 1].index].dwOutOctets)
            //  continue;       //连接列表中可能会有相同的连接，统计所有连接的网速时，忽略掉已发送和已接收字节数完全相同的连接
            m_in_bytes += table.dwInOctets;
            m_out_bytes += table.dwOutOctets;
        }
    }

    unsigned __int64 cur_in_speed{}, cur_out_speed{};       //本次监控时间间隔内的上传和下载速度

    //如果发送和接收的字节数为0或上次发送和接收的字节数为0或当前连接已改变时，网速无效
    if ((m_in_bytes == 0 && m_out_bytes == 0) || (m_last_in_bytes == 0 && m_last_out_bytes == 0) || m_connection_change_flag
        || m_last_in_bytes > m_in_bytes || m_last_out_bytes > m_out_bytes)
    {
        cur_in_speed = 0;
        cur_out_speed = 0;
    }
    else
    {
        cur_in_speed = m_in_bytes - m_last_in_bytes;
        cur_out_speed = m_out_bytes - m_last_out_bytes;
    }
    ////如果大于1GB/s，说明可能产生了异常，网速无效
    //if (cur_in_speed > 1073741824)
    //  cur_in_speed = 0;
    //if (cur_out_speed > 1073741824)
    //  cur_out_speed = 0;

    //计算两次获取网速的时间间隔
    static ULONGLONG last_net_speed_time = 0;
    ULONGLONG net_speed_time = GetTickCount64();
    int time_span = theApp.m_general_data.monitor_time_span;
    if (last_net_speed_time != 0)
        time_span = static_cast<int>((std::max)(ULONGLONG(1), net_speed_time - last_net_speed_time));
    last_net_speed_time = net_speed_time;

    //将当前监控时间间隔的流量转换成每秒时间间隔内的流量
    theApp.m_in_speed = static_cast<unsigned __int64>(cur_in_speed * 1000 / time_span);
    theApp.m_out_speed = static_cast<unsigned __int64>(cur_out_speed * 1000 / time_span);

    m_connection_change_flag = false;    //清除连接发生变化的标志

    m_last_in_bytes = m_in_bytes;
    m_last_out_bytes = m_out_bytes;

    //处于自动选择状态时，如果连续30秒没有网速，则可能自动选择的网络不对，此时执行一次自动选择
    if (theApp.m_cfg_data.m_auto_select)
    {
        if (cur_in_speed == 0 && cur_out_speed == 0)
            m_zero_speed_cnt++;
        else
            m_zero_speed_cnt = 0;
        if (m_zero_speed_cnt >= GetMonitorTimerCount(30))
        {
            AutoSelect();
            m_zero_speed_cnt = 0;
        }
    }

    //检测当前日期是否改变，如果已改变，则清零今天已使用的流量
    SYSTEMTIME current_time;
    GetLocalTime(&current_time);
    static int last_check_day = -1;  //用于检测日期变化
    if (last_check_day != current_time.wDay)
    {
        theApp.m_today_up_traffic = 0;
        theApp.m_today_down_traffic = 0;
        last_check_day = current_time.wDay;
    }

    //统计今天已使用的流量
    //（本程序已移除历史流量统计功能，今日流量仅在内存中累计，程序重启后归零）
    theApp.m_today_up_traffic += cur_out_speed;
    theApp.m_today_down_traffic += cur_in_speed;

    // Network selection, menus and snapshots are owned by the UI thread.
}

void CTaskbarCore::DoMonitorAcquisition()
{
    bool lite_version = false;
#ifdef WITHOUT_TEMPERATURE
    lite_version = true;
#endif

    bool cpu_freq_acquired = false;
    bool gpu_usage_acquired = false;
    m_get_disk_usage_by_pdh = false;

    //获取CPU使用率
    theApp.m_cpu_usage = m_cpu_usage_helper.GetCpuUsage(theApp.m_general_data.cpu_usage_acquire_method == GeneralSettingData::CA_CPU_TIME);

    //获取CPU频率
    //if (lite_version || is_arm64ec || !theApp.m_general_data.IsHardwareEnable(HI_CPU))
    //{
    if (m_cpu_freq_helper.GetCpuFreq(theApp.m_cpu_freq))
        cpu_freq_acquired = true;
    //}

    //获取GPU利用率
    if (lite_version /*|| is_arm64ec*/ || !theApp.m_general_data.IsHardwareEnable(HI_GPU))
    {
        if (m_gpu_usage_helper.GetGpuUsage(theApp.m_gpu_usage))
            gpu_usage_acquired = true;
        else
            theApp.m_gpu_usage = -1;
    }

    //获取硬盘利用率
    if (lite_version /*|| is_arm64ec*/ || !theApp.m_general_data.IsHardwareEnable(HI_HDD))
    {
        int disk_index = m_disk_usage_helper.FindDiskIndex(theApp.m_general_data.hard_disk_name);
        //没有找到要监控的硬盘时默认使用总体利用率
        if (disk_index < 0)
        {
            disk_index = m_disk_usage_helper.FindDiskIndex(L"_Total");
            if (disk_index >= 0)
            {
                theApp.m_general_data.hard_disk_name = L"_Total";
            }
            //仍然没有找到使用第1块硬盘
            else
            {
                const auto& disk_names = m_disk_usage_helper.GetDiskNames();
                if (!disk_names.empty())
                {
                    disk_index = 0;
                    theApp.m_general_data.hard_disk_name = disk_names.front();
                }
            }
        }
        if (m_disk_usage_helper.GetDiskUsage(disk_index, theApp.m_hdd_usage))
            m_get_disk_usage_by_pdh = true;
        else
            theApp.m_hdd_usage = -1;
    }

    //获取内存利用率
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    GlobalMemoryStatusEx(&statex);
    theApp.m_memory_usage = statex.dwMemoryLoad;
    theApp.m_used_memory = static_cast<int>((statex.ullTotalPhys - statex.ullAvailPhys) / 1024);
    theApp.m_total_memory = static_cast<int>(statex.ullTotalPhys / 1024);

#ifndef WITHOUT_TEMPERATURE
    //获取温度
    if (IsTemperatureNeeded() && theApp.m_pMonitor != nullptr)
    {
        CSingleLock sync(&theApp.m_minitor_lib_critical, TRUE);
        CString error_info = CCommon::LoadText(IDS_HARDWARE_INFO_ACQUIRE_FAILED_ERROR);

        auto getHardwareInfo = [&]() {
            __try
            {
                theApp.m_pMonitor->GetHardwareInfo();
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                AfxMessageBox(error_info, MB_ICONERROR | MB_OK);
            }
        };

        getHardwareInfo();
        auto monitor_error_message{ OpenHardwareMonitorApi::GetErrorMessage() };
        if (!monitor_error_message.empty())
        {
            AfxMessageBox(monitor_error_message.c_str(), MB_ICONERROR | MB_OK);
        }
        //theApp.m_cpu_temperature = theApp.m_pMonitor->CpuTemperature();
        theApp.m_gpu_temperature = theApp.m_pMonitor->GpuTemperature();
        //theApp.m_hdd_temperature = theApp.m_pMonitor->HDDTemperature();
        theApp.m_main_board_temperature = theApp.m_pMonitor->MainboardTemperature();
        if (!gpu_usage_acquired)
            theApp.m_gpu_usage = theApp.m_pMonitor->GpuUsage();
        if (!cpu_freq_acquired)
            theApp.m_cpu_freq = theApp.m_pMonitor->CpuFreq();
        //获取CPU温度
        if (!theApp.m_pMonitor->AllCpuTemperature().empty())
        {
            if (theApp.m_general_data.cpu_core_name == L"Core Average")  //如果选择了平均温度
            {
                theApp.m_cpu_temperature = theApp.m_pMonitor->CpuTemperature();
            }
            else
            {
                auto iter = theApp.m_pMonitor->AllCpuTemperature().find(theApp.m_general_data.cpu_core_name);
                if (iter == theApp.m_pMonitor->AllCpuTemperature().end())
                {
                    iter = theApp.m_pMonitor->AllCpuTemperature().begin();
                    theApp.m_general_data.cpu_core_name = iter->first;
                }
                theApp.m_cpu_temperature = iter->second;
            }
        }
        else
        {
            theApp.m_cpu_temperature = -1;
        }
        //获取硬盘温度
        if (!theApp.m_pMonitor->AllHDDTemperature().empty())
        {
            auto iter = theApp.m_pMonitor->AllHDDTemperature().find(theApp.m_general_data.hard_disk_name);
            if (iter == theApp.m_pMonitor->AllHDDTemperature().end())
            {
                iter = theApp.m_pMonitor->AllHDDTemperature().begin();
                theApp.m_general_data.hard_disk_name = iter->first;
            }
            theApp.m_hdd_temperature = iter->second;
        }
        else
        {
            theApp.m_hdd_temperature = -1;
        }
        //获取硬盘利用率
        if (!m_get_disk_usage_by_pdh)
        {
            if (!theApp.m_pMonitor->AllHDDUsage().empty())
            {
                auto iter = theApp.m_pMonitor->AllHDDUsage().find(theApp.m_general_data.hard_disk_name);
                if (iter == theApp.m_pMonitor->AllHDDUsage().end())
                {
                    iter = theApp.m_pMonitor->AllHDDUsage().begin();
                    theApp.m_general_data.hard_disk_name = iter->first;
                }
                theApp.m_hdd_usage = iter->second;
            }
            else
            {
                theApp.m_hdd_usage = -1;
            }
        }
    }
#endif

    Builtin::SampleHardware();
#ifdef WITHOUT_TEMPERATURE
    Builtin::SampleTemperatures(theApp.m_general_data.hardware_monitor_item, theApp.m_general_data.cpu_core_name,
        theApp.m_cpu_temperature, theApp.m_gpu_temperature);
#endif

    m_monitor_time_cnt++;

    //发送监控信息更新消息
    PostMessage(WM_MONITOR_INFO_UPDATED);
}

UINT CTaskbarCore::MonitorThreadCallback(LPVOID dwUser)
{
    CTaskbarCore* pThis = (CTaskbarCore*)dwUser;
    while (true)
    {
        //获取一次监控数据
        if (pThis->m_monitor_data_required)
        {
            pThis->DoMonitorAcquisition();
            //获取到监控数据后重置flag
            pThis->m_monitor_data_required = false;
        }
        else
        {
            Sleep(10);
        }

        // 检查退出标志
        if (pThis->m_is_thread_exit)
        {
            // 触发事件，通知主线程工作线程已退出
            pThis->m_threadExitEvent.SetEvent();
            return 0;
        }
    }

    return 0;
}


void CTaskbarCore::ExitMonitorThread()
{
    // 通知线程退出
    m_is_thread_exit = true;

    // 等待线程退出
    if (m_monitor_thread_started)
    {
        ::WaitForSingleObject(m_threadExitEvent.m_hObject, INFINITE);
        m_monitor_thread_started = false;
    }
}


void CTaskbarCore::OnTimer(UINT_PTR nIDEvent)
{
    // TODO: 在此添加消息处理程序代码和/或调用默认值
    if (nIDEvent == MONITOR_TIMER)
    {
        DoNetworkAcquisition();
        //通知线程获取硬件监控数据
        m_monitor_data_required = true;
    }

    if (nIDEvent == MAIN_TIMER)
    {
        m_timer_cnt++;
        if (m_first_start)      //这个if语句在程序启动后1秒执行
        {
            //打开任务栏窗口
            if (m_tBarDlg == nullptr)
                OpenTaskBarWnd();

            m_first_start = false;
        }


        //每隔10秒钟检测一次是否可以嵌入任务栏
        if (!theApp.m_win_version.IsWine() && IsTaskbarWndValid() && m_timer_cnt % 10 == 1)
        {
                if (m_tBarDlg->GetCannotInsertToTaskBar() && m_insert_to_taskbar_cnt < MAX_INSERT_TO_TASKBAR_CNT)
                {
                    CloseTaskBarWnd();
                    OpenTaskBarWnd();
                    m_insert_to_taskbar_cnt++;
                    if (m_tBarDlg->GetCannotInsertToTaskBar() && m_insert_to_taskbar_cnt >= WARN_INSERT_TO_TASKBAR_CNT)
                    {
                        //写入错误日志
                        CString info = CCommon::LoadText(IDS_CONNOT_INSERT_TO_TASKBAR_ERROR_LOG);
                        info.Replace(_T("<%cnt%>"), CCommon::IntToString(m_insert_to_taskbar_cnt));
                        info.Replace(_T("<%error_code%>"), CCommon::IntToString(m_tBarDlg->GetErrorCode()));
                        CCommon::WriteLog(info, theApp.m_log_path.c_str());
                        if (m_cannot_insert_to_task_bar_warning)      //确保提示信息只弹出一次
                        {
                            //弹出错误信息
                            m_cannot_insert_to_task_bar_warning = false;
                            MessageBox(CCommon::LoadText(IDS_CONNOT_INSERT_TO_TASKBAR, CCommon::IntToString(m_tBarDlg->GetErrorCode())), NULL, MB_ICONWARNING);
                        }
                    }
                }
                if (!m_tBarDlg->GetCannotInsertToTaskBar())
                {
                    m_insert_to_taskbar_cnt = 0;
                }
            }

        //检查是否需要弹出鼠标提示
        //setting_data: 消息提示的设置数据
        //value: 当前的值
        //last_value: 传递一个static或可以在此lambda表达式调用结束后继续存在的变量，用于保存上一次的值
        //notify_time: 传递一个static或可以在此lambda表达式调用结束后继续存在的变量，用于记录上次弹出提示的时间（定时器触发次数）
        //tip_str: 要提示的消息
        auto checkNotifyTip = [&](GeneralSettingData::NotifyTipSettings setting_data, int value, int& last_value, int& notify_time, LPCTSTR tip_str)
        {
            if (setting_data.enable)
            {
                if (last_value < setting_data.tip_value && value >= setting_data.tip_value && (m_timer_cnt - notify_time > static_cast<unsigned int>(theApp.m_notify_interval)))
                {
                    ShowNotifyTip(CCommon::LoadText(_T("SimpleTaskbarMonitor "), IDS_NOTIFY, _T("")), tip_str);
                    notify_time = m_timer_cnt;
                }
                last_value = value;
            }
        };

        //检查是否要弹出内存使用率超出提示
        CString info;
        info.Format(CCommon::LoadText(IDS_MEMORY_UDAGE_EXCEED, _T(" %d%%!")), theApp.m_memory_usage);
        static int last_memory_usage;
        static int memory_usage_notify_time{ -theApp.m_notify_interval };       //记录上次弹出提示时的时间
        checkNotifyTip(theApp.m_general_data.memory_usage_tip, theApp.m_memory_usage, last_memory_usage, memory_usage_notify_time, info.GetString());

        //检查是否要弹出CPU温度使用率超出提示
        info.Format(CCommon::LoadText(IDS_CPU_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_cpu_temperature));
        static int last_cpu_temp;
        static int cpu_temp_notify_time{ -theApp.m_notify_interval };       //记录上次弹出提示时的时间
        checkNotifyTip(theApp.m_general_data.cpu_temp_tip, theApp.m_cpu_temperature, last_cpu_temp, cpu_temp_notify_time, info.GetString());

        //检查是否要弹出显卡温度使用率超出提示
        info.Format(CCommon::LoadText(IDS_GPU_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_gpu_temperature));
        static int last_gpu_temp;
        static int gpu_temp_notify_time{ -theApp.m_notify_interval };       //记录上次弹出提示时的时间
        checkNotifyTip(theApp.m_general_data.gpu_temp_tip, theApp.m_gpu_temperature, last_gpu_temp, gpu_temp_notify_time, info.GetString());

        //检查是否要弹出硬盘温度使用率超出提示
        info.Format(CCommon::LoadText(IDS_HDD_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_hdd_temperature));
        static int last_hdd_temp;
        static int hdd_temp_notify_time{ -theApp.m_notify_interval };       //记录上次弹出提示时的时间
        checkNotifyTip(theApp.m_general_data.hdd_temp_tip, theApp.m_hdd_temperature, last_hdd_temp, hdd_temp_notify_time, info.GetString());

        //检查是否要弹出主板温度使用率超出提示
        info.Format(CCommon::LoadText(IDS_MBD_TEMPERATURE_EXCEED, _T(" %d°C!")), static_cast<int>(theApp.m_main_board_temperature));
        static int last_main_board_temp;
        static int main_board_temp_notify_time{ -theApp.m_notify_interval };        //记录上次弹出提示时的时间
        checkNotifyTip(theApp.m_general_data.mainboard_temp_tip, theApp.m_main_board_temperature, last_main_board_temp, main_board_temp_notify_time, info.GetString());


        //检查是否要弹出流量使用超出提示
        if (theApp.m_general_data.traffic_tip_enable)
        {
            static __int64 last_today_traffic;
            __int64 traffic_tip_value;
            if (theApp.m_general_data.traffic_tip_unit == 0)
                traffic_tip_value = static_cast<__int64>(theApp.m_general_data.traffic_tip_value) * 1024 * 1024;
            else
                traffic_tip_value = static_cast<__int64>(theApp.m_general_data.traffic_tip_value) * 1024 * 1024 * 1024;

            __int64 today_traffic = theApp.m_today_up_traffic + theApp.m_today_down_traffic;
            if (last_today_traffic < traffic_tip_value && today_traffic >= traffic_tip_value)
            {
                CString info = CCommon::LoadText(IDS_TODAY_TRAFFIC_EXCEED, CCommon::DataSizeToString(today_traffic));
                ShowNotifyTip(CCommon::LoadText(_T("SimpleTaskbarMonitor "), IDS_NOTIFY, _T("")), info.GetString());
            }
            last_today_traffic = today_traffic;
        }

        CWindowsSettingHelper::CheckWindows10LightTheme();        //每隔1秒钟检查一下当前系统是否为白色主题

        //根据当前Win10颜色模式自动切换任务栏颜色
        bool light_mode = CWindowsSettingHelper::IsWindows10LightTheme();
        if (theApp.m_last_light_mode != light_mode)
        {
            theApp.m_last_light_mode = light_mode;
            bool restart_taskbar_dlg{ false };
            if (theApp.m_taskbar_data.auto_adapt_light_theme)
            {
                int style_index = CWindowsSettingHelper::IsWindows10LightTheme() ? theApp.m_taskbar_data.light_default_style : theApp.m_taskbar_data.dark_default_style;
                theApp.m_taskbar_default_style.ApplyDefaultStyle(style_index, theApp.m_taskbar_data);
                theApp.SaveConfig();
                restart_taskbar_dlg = true;
            }
            bool is_taskbar_transparent{ theApp.m_taskbar_data.IsTaskbarTransparent()};
            if (!is_taskbar_transparent)
            {
                theApp.m_taskbar_data.SetTaskabrTransparent(false);
                restart_taskbar_dlg = true;
            }
            if (restart_taskbar_dlg && IsTaskbarWndValid())
            {
                m_tBarDlg->ApplyWindowTransparentColor();
                //CloseTaskBarWnd();
                //OpenTaskBarWnd();

                //写入调试日志
                if (theApp.m_debug_log)
                {
                    CString log_str;
                    log_str += _T("检测到 Windows10 深浅色变化。\n");
                    log_str += _T("IsWindows10LightTheme: ");
                    log_str += std::to_wstring(light_mode).c_str();
                    log_str += _T("\n");
                    log_str += _T("auto_adapt_light_theme: ");
                    log_str += std::to_wstring(theApp.m_taskbar_data.auto_adapt_light_theme).c_str();
                    log_str += _T("\n");
                    log_str += _T("is_taskbar_transparent: ");
                    log_str += std::to_wstring(is_taskbar_transparent).c_str();
                    log_str += _T("\n");
                    log_str += _T("taskbar_back_color: ");
                    log_str += std::to_wstring(theApp.m_taskbar_data.back_color).c_str();
                    log_str += _T("\n");
                    log_str += _T("taskbar_transparent_color: ");
                    log_str += std::to_wstring(theApp.m_taskbar_data.transparent_color).c_str();
                    log_str += _T("\n");
                    log_str += _T("taskbar_text_colors: ");
                    for (const auto& item : theApp.m_taskbar_data.text_colors)
                    {
                        log_str += std::to_wstring(item.second.label).c_str();
                        log_str += _T('|');
                        log_str += std::to_wstring(item.second.value).c_str();
                        log_str += _T(", ");
                    }
                    log_str += _T("\n");
                    CCommon::WriteLog(log_str, (theApp.m_config_dir + L".\\debug.log").c_str());
                }
            }

            //根据当前Win10颜色模式自动切换通知区图标
            if (theApp.m_cfg_data.m_notify_icon_auto_adapt)
            {
                int notify_icon_selected = theApp.m_cfg_data.m_notify_icon_selected;
                theApp.AutoSelectNotifyIcon();
                if (notify_icon_selected != theApp.m_cfg_data.m_notify_icon_selected)
                {
                    m_ntIcon.hIcon = theApp.m_notify_icons[theApp.m_cfg_data.m_notify_icon_selected];
                    if (theApp.m_general_data.show_notify_icon)
                    {
                        DeleteNotifyIcon();
                        AddNotifyIcon();
                    }
                }
            }

        }

        //根据任务栏颜色自动设置任务栏窗口背景色
        if (theApp.m_taskbar_data.auto_set_background_color && theApp.m_win_version.IsWindows8OrLater()
            && IsTaskbarWndValid() && theApp.m_taskbar_data.transparent_color != 0
            && theApp.m_taskbar_data.disable_d2d)
        {
            CRect rect;
            ::GetWindowRect(m_tBarDlg->GetSafeHwnd(), rect);
            int pointx{ rect.left - 1 };
            if (theApp.m_taskbar_data.tbar_wnd_on_left && m_tBarDlg->IsTasksbarOnTopOrBottom())
                pointx = rect.right + 1;
            int pointy = rect.bottom;
            if (pointx < 0) pointx = 0;
            if (pointx >= m_screen_size.cx) pointx = m_screen_size.cx - 1;
            if (pointy < 0) pointy = 0;
            if (pointy >= m_screen_size.cy) pointy = m_screen_size.cy - 1;
            COLORREF color = ::GetPixel(m_desktop_dc, pointx, pointy);        //取任务栏窗口左侧1像素处的颜色作为背景色
            if (!CCommon::IsColorSimilar(color, theApp.m_taskbar_data.back_color) && (/*CWindowsSettingHelper::IsWindows10LightTheme() ||*/ color != 0))
            {
                bool is_taskbar_transparent{ theApp.m_taskbar_data.IsTaskbarTransparent()};
                theApp.m_taskbar_data.back_color = color;
                theApp.m_taskbar_data.SetTaskabrTransparent(is_taskbar_transparent);
                if (is_taskbar_transparent)
                    m_tBarDlg->ApplyWindowTransparentColor();
            }
        }

        //当检测到背景色和文字颜色都为黑色写入错误日志
        static bool erro_log_write{ false };
        if (theApp.m_taskbar_data.back_color == 0 && !theApp.m_taskbar_data.text_colors.empty() && theApp.m_taskbar_data.text_colors.begin()->second.label == 0)
        {
            if (!erro_log_write)
            {
                CString log_str;
                log_str.Format(_T("检查到背景色和文字颜色都为黑色。IsWindows10LightTheme: %d, 系统启动时间：%d/%.2d/%.2d %.2d:%.2d:%.2d"),
                    light_mode, m_start_time.wYear, m_start_time.wMonth, m_start_time.wDay, m_start_time.wHour, m_start_time.wMinute, m_start_time.wSecond);
                CCommon::WriteLog(log_str, theApp.m_log_path.c_str());
                erro_log_write = true;
            }
        }
        else
        {
            erro_log_write = false;
        }

        UpdateNotifyIconTip();


    }

    if (nIDEvent == DELAY_TIMER)
    {
        AutoSelect();
        KillTimer(DELAY_TIMER);
    }

    if (nIDEvent == TASKBAR_TIMER)
    {
        ++m_taskbar_timer_cnt;
        if (m_taskbar_timer_cnt % 5 == 0 && theApp.m_taskbar_data.show_taskbar_wnd_in_secondary_display)
        {
            static int last_taskbar_num = 0;
            int taskbar_num = CTaskbarHelper::GetSecondaryTaskbarNum();
            //如果副显示器的任务栏数量发生变化，则重启任务栏窗口
            if (last_taskbar_num != taskbar_num)
            {
                last_taskbar_num = taskbar_num;
                //延迟一段时间后重启任务栏窗口
                KillTimer(RESTART_TASKBAR_TIMER);
                SetTimer(RESTART_TASKBAR_TIMER, 500, [](HWND, UINT, UINT_PTR, DWORD) {
                    theApp.m_pMainWnd->SendMessage(WM_REOPEN_TASKBAR_WND);
                    ::KillTimer(theApp.m_pMainWnd->GetSafeHwnd(), RESTART_TASKBAR_TIMER);
                });
            }
        }

        if (IsTaskbarWndValid())
        {
            //启动时就隐藏主窗体的情况下，无法收到dpichange消息，故需要手动检查
            //每次100ms*10执行一次屏幕DPI检查，并且尽可能少的检查操作系统版本
            if (m_taskbar_timer_cnt % 10 == 0 && theApp.m_win_version.IsWindows8Point1OrLater())
            {
                CTaskBarDlg::CheckWindowMonitorDPIAndHandle(*m_tBarDlg, [p_TaskBarDlg = m_tBarDlg](UINT new_dpi_x, UINT new_dpi_y)
                                                            {
                                                                // auto s_dpi = std::to_string(new_dpi_x);
                                                                // s_dpi += '\n';
                                                                // TRACE(s_dpi.c_str());
                                                                //考虑到任务栏窗口可能和主窗口不在同一个屏幕上，dpi可能不同
                                                                //设置DPI并刷新窗口
                                                                p_TaskBarDlg->SetDPI(new_dpi_x);
                                                                p_TaskBarDlg->SetTextFont();
                                                                p_TaskBarDlg->CalculateWindowSize(); });
            }

            m_tBarDlg->AdjustWindowPos();
            m_tBarDlg->Invalidate(FALSE);
        }
    }

    if (nIDEvent == DELETE_NOTIFY_ICON_TIMER)
    {
        DeleteNotifyIcon();
        KillTimer(DELETE_NOTIFY_ICON_TIMER);
    }

    CDialog::OnTimer(nIDEvent);
}




void CTaskbarCore::OnClose()
{
    theApp.m_cannot_save_config_warning = true;
    theApp.m_cannot_save_global_config_warning = true;
    theApp.SaveConfig();
    theApp.SaveGlobalConfig();

    for (const auto& item : CBaseDialog::AllUniqueHandels())
        ::SendMessage(item.second, WM_COMMAND, IDCANCEL, 0);

    // The core is modeless: destroy it instead of calling EndDialog.
    DestroyWindow();
}


BOOL CTaskbarCore::OnCommand(WPARAM wParam, LPARAM lParam)
{
    UINT uMsg = LOWORD(wParam);
    if (uMsg == ID_SELECT_ALL_CONNECTION)
    {
        theApp.m_cfg_data.m_select_all = true;
        theApp.m_cfg_data.m_auto_select = false;
        m_connection_change_flag = true;
    }
    //选择了“选择网络连接”子菜单中项目时的处理
    if (uMsg == ID_SELETE_CONNECTION)   //选择了“自动选择”菜单项
    {
        AutoSelect();
        theApp.m_cfg_data.m_auto_select = true;
        theApp.m_cfg_data.m_select_all = false;
        theApp.SaveConfig();
        m_connection_change_flag = true;
    }
    if (uMsg > ID_SELECT_ALL_CONNECTION && uMsg <= ID_SELECT_ALL_CONNECTION + m_connections.size()) //选择了一个网络连接
    {
        m_connection_selected = uMsg - ID_SELECT_ALL_CONNECTION - 1;
        theApp.m_cfg_data.m_connection_name = GetConnection(m_connection_selected).description_2;
        m_connection_name_preferd = theApp.m_cfg_data.m_connection_name;
        theApp.m_cfg_data.m_auto_select = false;
        theApp.m_cfg_data.m_select_all = false;
        theApp.SaveConfig();
        m_connection_change_flag = true;
    }

    return CDialog::OnCommand(wParam, lParam);
}




BOOL CTaskbarCore::PreTranslateMessage(MSG* pMsg)
{
    //屏蔽按回车键和ESC键退出
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_ESCAPE) return TRUE;
    if (pMsg->message == WM_KEYDOWN && pMsg->wParam == VK_RETURN) return TRUE;

    return CDialog::PreTranslateMessage(pMsg);
}





afx_msg LRESULT CTaskbarCore::OnNotifyIcon(WPARAM wParam, LPARAM lParam)
{
    bool dialog_exist{ false };
    HWND handle{};
    if (lParam == WM_LBUTTONDOWN || lParam == WM_RBUTTONUP || lParam == WM_LBUTTONDBLCLK)
    {
        for (const auto& item : CBaseDialog::AllUniqueHandels())
        {
            if (IsWindow(item.second))
            {
                dialog_exist = true;
                handle = item.second;
                break;
            }
        }
    }

    if (lParam == WM_LBUTTONDOWN || lParam == WM_LBUTTONDBLCLK)
    {
        if (dialog_exist)       //有打开的对话框时，点击通知区图标后将焦点设置到对话框
        {
            ::SetForegroundWindow(handle);
        }
        else        //没有打开的对话框时，则显示任务栏窗口
        {
            if (!IsTaskbarWndValid())
            {
                OpenTaskBarWnd();
            }
            if (IsTaskbarWndValid())
                m_tBarDlg->SetForegroundWindow();
        }
    }
    if (lParam == WM_RBUTTONUP)
    {
        if (dialog_exist)       //有打开的对话框时，点击通知区图标后将焦点设置到对话框
        {
            ::SetForegroundWindow(handle);
        }
        else
        {
            //在通知区点击右键弹出右键菜单
            if (IsTaskbarWndValid())        //如果显示了任务栏窗口，则在右击了通知区图标后将焦点设置到任务栏窗口
                m_tBarDlg->SetForegroundWindow();
            CPoint point1;  //定义一个用于确定光标位置的位置
            GetCursorPos(&point1);  //获取当前光标的位置，以便使得菜单可以跟随光标
            CMenu* pMenu = theApp.m_taskbar_menu.GetSubMenu(0);
            if (pMenu != nullptr)
            {
                pMenu->SetDefaultItem(-1);       //设置没有默认菜单项
                m_menu_popuped = true;
                pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON, point1.x, point1.y, this); //在指定位置显示弹出菜单
                m_menu_popuped = false;
            }
        }
    }
    return 0;
}


void CTaskbarCore::OnShowNotifyIcon()
{
    // TODO: 在此添加命令处理程序代码
    if (theApp.m_general_data.show_notify_icon)
    {
        DeleteNotifyIcon();
        theApp.m_general_data.show_notify_icon = false;
    }
    else
    {
        AddNotifyIcon();
        theApp.m_general_data.show_notify_icon = true;
    }
    theApp.SaveConfig();
}


void CTaskbarCore::OnDestroy()
{
    KillTimer(MAIN_TIMER);
    KillTimer(MONITOR_TIMER);
    KillTimer(TASKBAR_TIMER);
    ExitMonitorThread();
    CloseTaskBarWnd();
    // Do not use DeleteNotifyIcon here: it recreates the taskbar window.
    ::Shell_NotifyIcon(NIM_DELETE, &m_ntIcon);
    CDialog::OnDestroy();
    ::PostQuitMessage(0);
}




//任务栏窗口切换显示CPU和内存利用率时的处理
void CTaskbarCore::OnShowCpuMemory2()
{
    // TODO: 在此添加命令处理程序代码
    if (IsTaskbarWndValid())
    {
        bool show_cpu_memory = (theApp.m_taskbar_data.display_item.Contains(TDI_CPU) || theApp.m_taskbar_data.display_item.Contains(TDI_MEMORY));
        if (show_cpu_memory)
        {
            theApp.m_taskbar_data.display_item.Remove(TDI_CPU);
            theApp.m_taskbar_data.display_item.Remove(TDI_MEMORY);
        }
        else
        {
            theApp.m_taskbar_data.display_item.Add(TDI_CPU);
            theApp.m_taskbar_data.display_item.Add(TDI_MEMORY);
        }
        //theApp.m_cfg_data.m_tbar_show_cpu_memory = !theApp.m_cfg_data.m_tbar_show_cpu_memory;
        //切换显示CPU和内存利用率时，删除任务栏窗口，再重新显示
        //CloseTaskBarWnd();
        //OpenTaskBarWnd();
        m_tBarDlg->WidthChanged();
    }
}




void CTaskbarCore::OnAppAbout()
{
    // TODO: 在此添加命令处理程序代码
    //弹出“关于”对话框
    CAboutDlg aDlg;
    aDlg.DoModal();
}


//当资源管理器重启时会触发此消息
LRESULT CTaskbarCore::OnTaskBarCreated(WPARAM wParam, LPARAM lParam)
{
    if (m_tBarDlg != nullptr)
    {
        CloseTaskBarWnd();
        if (theApp.m_general_data.show_notify_icon)
        {
            //重新添加通知栏图标
            ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon);
        }
        OpenTaskBarWnd();
    }
    else
    {
        if (theApp.m_general_data.show_notify_icon)
            ::Shell_NotifyIcon(NIM_ADD, &m_ntIcon);
    }
    return LRESULT();
}




void CTaskbarCore::OnOptions()
{
    _OnOptions(0, this);
}


//通过任务栏窗口的右键菜单打开“选项”对话框
void CTaskbarCore::OnOptions2()
{
    CWnd* pParent = this;
    if (IsTaskbarWndValid())
        pParent = m_tBarDlg;
    _OnOptions(0, pParent);
}


afx_msg LRESULT CTaskbarCore::OnExitmenuloop(WPARAM wParam, LPARAM lParam)
{
    m_menu_popuped = false;
    return 0;
}


void CTaskbarCore::OnChangeNotifyIcon()
{
    // TODO: 在此添加命令处理程序代码
    CIconSelectDlg dlg(theApp.m_cfg_data.m_notify_icon_selected);
    dlg.SetAutoAdaptNotifyIcon(theApp.m_cfg_data.m_notify_icon_auto_adapt);
    if (dlg.DoModal() == IDOK)
    {
        theApp.m_cfg_data.m_notify_icon_selected = dlg.GetIconSelected();
        theApp.m_cfg_data.m_notify_icon_auto_adapt = dlg.AutoAdaptNotifyIcon();
        m_ntIcon.hIcon = theApp.m_notify_icons[theApp.m_cfg_data.m_notify_icon_selected];
        if (theApp.m_cfg_data.m_notify_icon_auto_adapt)
            theApp.AutoSelectNotifyIcon();
        if (theApp.m_general_data.show_notify_icon)
        {
            DeleteNotifyIcon();
            AddNotifyIcon();
        }
        theApp.SaveConfig();
    }
}




afx_msg LRESULT CTaskbarCore::OnTaskbarMenuPopedUp(WPARAM wParam, LPARAM lParam)
{
    m_menu_popuped = true;
    //设置“选择连接”子菜单项中各单选项的选择状态
    SetConnectionMenuState(theApp.m_taskbar_menu.GetSubMenu(0)->GetSubMenu(0));
    SetConnectionMenuState(theApp.m_taskbar_menu_plugin.GetSubMenu(0)->GetSubMenu(0));
    return 0;
}


//任务栏窗口切换显示网速时的处理
void CTaskbarCore::OnShowNetSpeed()
{
    // TODO: 在此添加命令处理程序代码
    if (IsTaskbarWndValid())
    {
        bool show_net_speed = (theApp.m_taskbar_data.display_item.Contains(TDI_UP) || theApp.m_taskbar_data.display_item.Contains(TDI_DOWN));
        if (show_net_speed)
        {
            theApp.m_taskbar_data.display_item.Remove(TDI_UP);
            theApp.m_taskbar_data.display_item.Remove(TDI_DOWN);
        }
        else
        {
            theApp.m_taskbar_data.display_item.Add(TDI_UP);
            theApp.m_taskbar_data.display_item.Add(TDI_DOWN);
        }
        //CloseTaskBarWnd();
        //OpenTaskBarWnd();
        m_tBarDlg->WidthChanged();
    }
}


BOOL CTaskbarCore::OnQueryEndSession()
{
    if (!CDialog::OnQueryEndSession())
        return FALSE;

    // TODO:  在此添加专用的查询结束会话代码
    theApp.SaveConfig();
    theApp.SaveGlobalConfig();

    if (theApp.m_debug_log)
    {
        CCommon::WriteLog(_T("TrafficMonitor进程已被终止，设置已保存。"), (theApp.m_config_dir + L".\\debug.log").c_str());
    }

    return TRUE;
}




afx_msg LRESULT CTaskbarCore::OnDpichanged(WPARAM wParam, LPARAM lParam)
{
    static int dpi;
    static CTaskbarCore* pThis;
    dpi = LOWORD(wParam);
    pThis = this;

    //由于当任务栏窗口移动到不同DPI的显示器上时，会短时间内触发多次DPI更改消息，因此这里在收到消息后延迟一段时间后再处理
    KillTimer(DPI_CHANGE_TIMER);
    SetTimer(DPI_CHANGE_TIMER, 500, [](HWND, UINT, UINT_PTR, DWORD) {
        theApp.SetDPI(dpi);

        //当系统版本小于Windows 8.1时使用原来的行为
        if (pThis->IsTaskbarWndValid() && !theApp.m_win_version.IsWindows8Point1OrLater())
        {
            //为任务栏窗口重新指定DPI
            pThis->m_tBarDlg->SetDPI(dpi);
            //根据新的DPI重新设置任务栏窗口字体
            pThis->m_tBarDlg->SetTextFont();
        }

        pThis->KillTimer(DPI_CHANGE_TIMER);
    });

    return 0;
}


afx_msg LRESULT CTaskbarCore::OnMonitorInfoUpdated(WPARAM wParam, LPARAM lParam)
{
    //通知插件获取数据，以及向插件传递监控数据
    for (const auto& plugin_info : theApp.m_plugins.GetPlugins())
    {
        if (plugin_info.plugin != nullptr)
        {
            plugin_info.plugin->DataRequired();
            ITMPlugin::MonitorInfo monitor_info;
            monitor_info.up_speed = theApp.m_out_speed;
            monitor_info.down_speed = theApp.m_in_speed;
            monitor_info.cpu_usage = theApp.m_cpu_usage;
            monitor_info.memory_usage = theApp.m_memory_usage;
            monitor_info.gpu_usage = theApp.m_gpu_usage;
            monitor_info.hdd_usage = theApp.m_hdd_usage;
            monitor_info.cpu_temperature = theApp.m_cpu_temperature;
            monitor_info.gpu_temperature = theApp.m_gpu_temperature;
            monitor_info.hdd_temperature = theApp.m_hdd_temperature;
            monitor_info.cpu_freq = theApp.m_cpu_freq;
            monitor_info.main_board_temperature = theApp.m_main_board_temperature;
            plugin_info.plugin->OnMonitorInfo(monitor_info);
        }
    }

    //更新任务栏窗口鼠标提示
    if (IsTaskbarWndValid())
        m_tBarDlg->UpdateToolTips();
    return 0;
}


LRESULT CTaskbarCore::OnDisplaychange(WPARAM wParam, LPARAM lParam)
{
    GetScreenSize();
    return 0;
}




void CTaskbarCore::OnPluginManage()
{
    // TODO: 在此添加命令处理程序代码
    CPluginManagerDlg dlg;
    dlg.DoModal();
}

LRESULT CTaskbarCore::OnReopenTaksbarWnd(WPARAM wParam, LPARAM lParam)
{
    CloseTaskBarWnd();
    OpenTaskBarWnd();
    return 0;
}


void CTaskbarCore::OnOpenTaskManager()
{
    ShellExecuteW(NULL, _T("open"), (theApp.m_system_dir + L"\\Taskmgr.exe").c_str(), NULL, NULL, SW_NORMAL);       //打开任务管理器
}


afx_msg LRESULT CTaskbarCore::OnSettingsApplied(WPARAM wParam, LPARAM lParam)
{
    COptionsDlg* pOptionsDlg = (COptionsDlg*)wParam;
    if (pOptionsDlg != nullptr)
    {
        ApplySettings(*pOptionsDlg);
    }
    return 0;
}


void CTaskbarCore::OnDisplaySettings()
{
    // TODO: 在此添加命令处理程序代码
    CSetItemOrderDlg dlg;
    dlg.SetItemOrder(theApp.m_taskbar_data.item_order.GetItemOrderConst());
    dlg.SetDisplayItem(theApp.m_taskbar_data.display_item);
    dlg.SetPluginDisplayItem(theApp.m_taskbar_data.plugin_display_item);
    if (dlg.DoModal() == IDOK)
    {
        theApp.m_taskbar_data.item_order.SetOrder(dlg.GetItemOrder());
        theApp.m_taskbar_data.display_item = dlg.GetDisplayItem();
        theApp.m_taskbar_data.plugin_display_item = dlg.GetPluginDisplayItem();
        //CloseTaskBarWnd();
        //OpenTaskBarWnd();
        if (IsTaskbarWndValid())
            m_tBarDlg->WidthChanged();
    }
}




void CTaskbarCore::OnRefreshConnectionList()
{
    IniConnection();

}


afx_msg LRESULT CTaskbarCore::OnTabletQuerysystemgesturestatus(WPARAM wParam, LPARAM lParam)
{
    return 0;
}




void CTaskbarCore::OnPluginOptionsTaksbar()
{
    //判断任务栏窗口中点击的项目是否是插件项目
    if (IsTaskbarWndValid() && m_tBarDlg->GetClickedItem().IsPlugin())
    {
        //找到对应的插件
        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_tBarDlg->GetClickedItem().PluginItem());
        if (plugin != nullptr)
        {
            //显示插件的选项设置
            auto rtn = plugin->ShowOptionsDialog(m_tBarDlg->GetSafeHwnd());
            if (rtn == ITMPlugin::OR_OPTION_CHANGED)    //选项设置有更改，重新打开任务栏窗口
            {
                //CloseTaskBarWnd();
                //OpenTaskBarWnd();
                m_tBarDlg->WidthChanged();
            }
            if (rtn == ITMPlugin::OR_OPTION_NOT_PROVIDED)
                MessageBox(CCommon::LoadText(IDS_PLUGIN_NO_OPTIONS_INFO), nullptr, MB_ICONINFORMATION | MB_OK);
        }
    }
}


void CTaskbarCore::OnPluginDetailTaksbar()
{
    if (IsTaskbarWndValid() && m_tBarDlg->GetClickedItem().IsPlugin())
    {
        //找到对应的插件
        ITMPlugin* plugin = theApp.m_plugins.GetPluginByItem(m_tBarDlg->GetClickedItem().PluginItem());
        if (plugin != nullptr)
        {
            int index = theApp.m_plugins.GetPluginIndex(plugin);
            CPluginInfoDlg dlg(index);
            dlg.DoModal();
        }
    }
}


UINT CTaskbarCore::OnPowerBroadcast(UINT nPowerEvent, LPARAM nEventData)
{
    // 系统从休眠恢复
    if (nPowerEvent == PBT_APMRESUMESUSPEND)
    {
        //延迟一段时间后重新初始化网络连接
        KillTimer(INIT_CONNECT_TIMER);
        static CTaskbarCore* pThis = this;
        static int check_times = 0;
        SetTimer(INIT_CONNECT_TIMER, 10000, [](HWND, UINT, UINT_PTR, DWORD) {
            pThis->IniConnection();
            check_times++;

            //写入日志
            CString info = CCommon::LoadTextFormat(IDS_RESTORE_FROM_SLEEP_LOG, {pThis->m_restart_cnt });
            CCommon::WriteLog(info, theApp.m_log_path.c_str());

            //如果连接为空，定时器继续运行，每隔一段时间重新初始化连接
            if (pThis->m_connections.size() == 0)
            {
                //超过20次，结束定时器
                if (check_times >= 20)
                    pThis->KillTimer(INIT_CONNECT_TIMER);
            }
            //成功获取到连接，结束定时器
            else
            {
                pThis->KillTimer(INIT_CONNECT_TIMER);
                check_times = 0;
            }
        });
    }
    return CDialog::OnPowerBroadcast(nPowerEvent, nEventData);
}


void CTaskbarCore::OnColorizationColorChanged(DWORD dwColorizationColor, BOOL bOpacity)
{
    // 此功能要求 Windows Vista 或更高版本。
    // _WIN32_WINNT 符号必须 >= 0x0600。
    // TODO: 在此添加消息处理程序代码和/或调用默认值

    static DWORD last_color;
    if (last_color != dwColorizationColor)
    {
        last_color = dwColorizationColor;
        BYTE red = (dwColorizationColor >> 16) & 0xFF;
        BYTE green = (dwColorizationColor >> 8) & 0xFF;
        BYTE blue = dwColorizationColor & 0xFF;
        COLORREF theme_color = RGB(red, green, blue);
        theApp.SetThemeColor(theme_color);
    }
    CDialog::OnColorizationColorChanged(dwColorizationColor, bOpacity);
}
