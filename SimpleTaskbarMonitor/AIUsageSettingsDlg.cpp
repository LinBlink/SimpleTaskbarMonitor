#include "stdafx.h"
#include "TrafficMonitor.h"
#include "AIUsageSettingsDlg.h"
#include "Builtin/BuiltinFeatures.h"

IMPLEMENT_DYNAMIC(CAIUsageSettingsDlg, CTabDlg)

CAIUsageSettingsDlg::CAIUsageSettingsDlg(CWnd* pParent)
    : CTabDlg(IDD_AI_USAGE_SETTINGS_DIALOG, pParent)
{
}

std::wstring CAIUsageSettingsDlg::SettingsPath() const
{
    return Builtin::DataDirectory() + L"\\AIUsage.ini";
}

std::wstring CAIUsageSettingsDlg::CachePath() const
{
    return Builtin::DataDirectory() + L"\\AIUsage.cache.ini";
}

void CAIUsageSettingsDlg::DoDataExchange(CDataExchange* pDX)
{
    CTabDlg::DoDataExchange(pDX);
    DDX_Control(pDX, IDC_AI_USAGE_REFRESH_EDIT, m_refresh_edit);
    DDX_Control(pDX, IDC_AI_USAGE_MINIMAX_REGION_COMBO, m_region_combo);
}

BOOL CAIUsageSettingsDlg::OnInitDialog()
{
    CTabDlg::OnInitDialog();
    const auto path = SettingsPath();
    const UINT ids[] = {
        IDC_AI_USAGE_ENABLE_CHECK, IDC_AI_USAGE_MINIMAX_CHECK,
        IDC_AI_USAGE_CODEX_CHECK, IDC_AI_USAGE_CURSOR_CHECK,
        IDC_AI_USAGE_CLAUDE_CHECK, IDC_AI_USAGE_KIMI_CHECK,
        IDC_AI_USAGE_ANTIGRAVITY_CHECK
    };
    const wchar_t* keys[] = {
        L"EnableQueries", L"EnableMiniMax", L"EnableCodex", L"EnableCursor",
        L"EnableClaude", L"EnableKimi", L"EnableAntigravity"
    };
    for (size_t i = 0; i < _countof(ids); ++i)
        CheckDlgButton(ids[i], GetPrivateProfileIntW(L"settings", keys[i], 1, path.c_str()) ? BST_CHECKED : BST_UNCHECKED);
    m_refresh_edit.SetRange(60, 3600);
    m_refresh_edit.SetValue(GetPrivateProfileIntW(L"settings", L"RefreshSeconds", 180, path.c_str()));
    m_region_combo.AddString(L"cn");
    m_region_combo.AddString(L"global");
    wchar_t region[32]{};
    GetPrivateProfileStringW(L"settings", L"MiniMaxRegion", L"cn", region, _countof(region), path.c_str());
    m_region_combo.SetCurSel(_wcsicmp(region, L"global") == 0 ? 1 : 0);
    wchar_t model[256]{};
    GetPrivateProfileStringW(L"settings", L"MiniMaxModel", L"general", model, _countof(model), path.c_str());
    SetDlgItemTextW(IDC_AI_USAGE_MINIMAX_MODEL_EDIT, model);
    UpdateStatus();
    return TRUE;
}

void CAIUsageSettingsDlg::UpdateStatus()
{
    const auto path = CachePath();
    const wchar_t* names[] = { L"MiniMax 5h", L"MiniMax week", L"Codex 5h", L"Codex week", L"Cursor", L"Claude 5h", L"Claude week", L"Kimi 5h", L"Kimi week/month", L"Antigravity" };
    const wchar_t* keys[] = { L"minimax5h", L"minimaxWeek", L"codex5h", L"codexWeek", L"cursorPlan", L"claude5h", L"claudeWeek", L"kimi5h", L"kimiWeek", L"antigravityPlan" };
    CString text;
    for (size_t i = 0; i < _countof(keys); ++i)
    {
        wchar_t value[64]{}, status[512]{};
        GetPrivateProfileStringW(keys[i], L"value", L"--", value, _countof(value), path.c_str());
        GetPrivateProfileStringW(keys[i], L"status", L"Waiting for first query", status, _countof(status), path.c_str());
        CString line;
        line.Format(L"%s: %s | %s\r\n", names[i], value, status);
        text += line;
    }
    SetDlgItemTextW(IDC_AI_USAGE_STATUS_EDIT, text);
}

bool CAIUsageSettingsDlg::SaveSettings()
{
    const auto path = SettingsPath();
    const UINT ids[] = {
        IDC_AI_USAGE_ENABLE_CHECK, IDC_AI_USAGE_MINIMAX_CHECK,
        IDC_AI_USAGE_CODEX_CHECK, IDC_AI_USAGE_CURSOR_CHECK,
        IDC_AI_USAGE_CLAUDE_CHECK, IDC_AI_USAGE_KIMI_CHECK,
        IDC_AI_USAGE_ANTIGRAVITY_CHECK
    };
    const wchar_t* keys[] = {
        L"EnableQueries", L"EnableMiniMax", L"EnableCodex", L"EnableCursor",
        L"EnableClaude", L"EnableKimi", L"EnableAntigravity"
    };
    bool saved = true;
    for (size_t i = 0; i < _countof(ids); ++i)
        saved = WritePrivateProfileStringW(L"settings", keys[i], IsDlgButtonChecked(ids[i]) ? L"1" : L"0", path.c_str()) && saved;
    const int refresh = std::clamp(m_refresh_edit.GetValue(), 60, 3600);
    m_refresh_edit.SetValue(refresh);
    saved = WritePrivateProfileStringW(L"settings", L"RefreshSeconds", std::to_wstring(refresh).c_str(), path.c_str()) && saved;
    CString region, model;
    m_region_combo.GetWindowTextW(region);
    GetDlgItemTextW(IDC_AI_USAGE_MINIMAX_MODEL_EDIT, model);
    model.Trim();
    if (model.IsEmpty()) model = L"general";
    saved = WritePrivateProfileStringW(L"settings", L"MiniMaxRegion", region, path.c_str()) && saved;
    saved = WritePrivateProfileStringW(L"settings", L"MiniMaxModel", model, path.c_str()) && saved;
    saved = WritePrivateProfileStringW(L"settings", L"MiniMaxCountMeaning", L"remaining", path.c_str()) && saved;
    if (!saved)
        MessageBox(L"Could not save AI Usage settings. Check that the program directory is writable.", APP_NAME, MB_OK | MB_ICONERROR);
    return saved;
}

void CAIUsageSettingsDlg::OnOK()
{
    SaveSettings();
}

void CAIUsageSettingsDlg::OnSettingsApplied()
{
    SaveSettings();
}

void CAIUsageSettingsDlg::OnBnClickedRefresh()
{
    if (SaveSettings())
    {
        Builtin::RequestAIUsageRefresh();
        SetTimer(1, 1000, nullptr);
    }
}

void CAIUsageSettingsDlg::OnBnClickedMiniMaxKey()
{
    wchar_t system[32768]{};
    GetSystemDirectoryW(system, _countof(system));
    const auto directory = Builtin::DataDirectory();
    const auto executable = std::wstring(system) + L"\\WindowsPowerShell\\v1.0\\powershell.exe";
    const auto arguments = L"-NoLogo -NoProfile -ExecutionPolicy Bypass -File \"" + directory + L"\\AIUsage.Setup.ps1\"";
    const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(m_hWnd, L"open", executable.c_str(), arguments.c_str(), directory.c_str(), SW_SHOWNORMAL));
    if (result <= 32)
        MessageBox(L"Could not open MiniMax subscription key setup.", APP_NAME, MB_OK | MB_ICONERROR);
}

void CAIUsageSettingsDlg::OnTimer(UINT_PTR nIDEvent)
{
    if (nIDEvent == 1) UpdateStatus();
    CTabDlg::OnTimer(nIDEvent);
}

BEGIN_MESSAGE_MAP(CAIUsageSettingsDlg, CTabDlg)
    ON_BN_CLICKED(IDC_AI_USAGE_REFRESH_BUTTON, &CAIUsageSettingsDlg::OnBnClickedRefresh)
    ON_BN_CLICKED(IDC_AI_USAGE_MINIMAX_KEY_BUTTON, &CAIUsageSettingsDlg::OnBnClickedMiniMaxKey)
    ON_WM_TIMER()
END_MESSAGE_MAP()
