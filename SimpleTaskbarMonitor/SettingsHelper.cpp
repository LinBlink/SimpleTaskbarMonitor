#include "stdafx.h"
#include "SettingsHelper.h"
#include "TrafficMonitor.h"
#include "StableItemColors.h"
#include "Builtin/AIUsageLayout.h"

CSettingsHelper::CSettingsHelper()
    : CIniHelper(theApp.m_config_path)
{
}

CSettingsHelper::CSettingsHelper(const std::wstring& file_path)
    : CIniHelper(file_path)
{
}

void CSettingsHelper::SaveFontData(const wchar_t* AppName, const FontInfo& font)
{
    WriteString(AppName, L"font_name", wstring(font.name));
    WriteInt(AppName, L"font_size", font.size);
    bool style[4];
    style[0] = font.bold;
    style[1] = font.italic;
    style[2] = font.underline;
    style[3] = font.strike_out;
    WriteBoolArray(AppName, L"font_style", style, 4);
}

void CSettingsHelper::LoadFontData(const wchar_t* AppName, FontInfo& font, const FontInfo& default_font) const
{
    font.name = GetString(AppName, L"font_name", default_font.name).c_str();
    font.size = GetInt(AppName, L"font_size", default_font.size);
    bool style[4];
    GetBoolArray(AppName, L"font_style", style, 4);
    font.bold = style[0];
    font.italic = style[1];
    font.underline = style[2];
    font.strike_out = style[3];
}

void CSettingsHelper::LoadMainWndColors(const wchar_t* AppName, const wchar_t* KeyName, const std::set<CommonDisplayItem>& all_items, std::map<CommonDisplayItem, COLORREF>& text_colors, COLORREF default_color)
{
    CString default_str;
    default_str.Format(_T("%d"), default_color);
    wstring str{ default_str.GetString() };
    _GetString(AppName, KeyName, str);
    std::vector<wstring> split_result;
    CCommon::StringSplit(str, L',', split_result);
    size_t index = 0;
    const std::set<CommonDisplayItem>& items{ all_items.empty() ? theApp.m_plugins.AllDisplayItemsWithPlugins() : all_items };
    for (auto iter = items.begin(); iter != items.end(); ++iter)
    {
        if (index < split_result.size())
            text_colors[*iter] = _wtoi(split_result[index].c_str());
        else if (!split_result.empty())
            text_colors[*iter] = _wtoi(split_result[0].c_str());
        else
            text_colors[*iter] = default_color;
        index++;
    }
}

void CSettingsHelper::SaveMainWndColors(const wchar_t* AppName, const wchar_t* KeyName, const std::map<CommonDisplayItem, COLORREF>& text_colors)
{
    CString str;
    for (auto iter = text_colors.begin(); iter != text_colors.end(); ++iter)
    {
        CString tmp;
        tmp.Format(_T("%d,"), iter->second);
        str += tmp;
    }
    _WriteString(AppName, KeyName, wstring(str));
}

void CSettingsHelper::LoadTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, std::map<CommonDisplayItem, TaskbarItemColor>& text_colors, const wchar_t* default_str)
{
    wstring str{ default_str };
    _GetString(AppName, KeyName, str);
    std::vector<wstring> split_result;
    CCommon::StringSplit(str, L',', split_result);
    std::vector<wstring> saved_ids, current_ids;
    CCommon::StringSplit(GetString(AppName, (std::wstring(KeyName) + L"_item_ids").c_str(), L""), L',', saved_ids);
    for (const auto& item : theApp.m_plugins.AllDisplayItemsWithPlugins())
        current_ids.push_back(item.GetItemIniKeyName());
    const auto colors = StableItemColors::Decode(split_result, saved_ids, current_ids, _wtoi(default_str));
    size_t index = 0;
    for (const auto& item : theApp.m_plugins.AllDisplayItemsWithPlugins())
    {
        text_colors[item].label = colors[index].label;
        text_colors[item].value = colors[index].value;
        ++index;
    }
}

void CSettingsHelper::LoadTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, std::map<CommonDisplayItem, TaskbarItemColor>& text_colors, COLORREF default_color)
{
    CString default_str;
    default_str.Format(_T("%d"), default_color);
    LoadTaskbarWndColors(AppName, KeyName, text_colors, default_str.GetString());
}

void CSettingsHelper::SaveTaskbarWndColors(const wchar_t* AppName, const wchar_t* KeyName, const std::map<CommonDisplayItem, TaskbarItemColor>& text_colors)
{
    CString str;
    std::wstring ids;
    for (const auto& entry : text_colors)
    {
        CString tmp;
        tmp.Format(_T("%d,%d,"), entry.second.label, entry.second.value);
        str += tmp;
        if (!ids.empty()) ids += L",";
        ids += entry.first.GetItemIniKeyName();
    }
    _WriteString(AppName, KeyName, wstring(str));
    WriteString(AppName, (std::wstring(KeyName) + L"_item_ids").c_str(), ids);
}

void CSettingsHelper::LoadDisplayStr(const wchar_t* AppName, DispStrings& disp_str, bool is_main_window) const
{
    for (auto display_item : AllDisplayItems)
    {
        std::wstring str{ CommonDisplayItem(display_item).DefaultString(is_main_window)};
        bool exist = GetString(AppName, CommonDisplayItem(display_item).GetItemIniKeyName(), str);
        //主窗口只读取配置文件中存在的项，任务栏窗口读取所有项
        if (!is_main_window || exist)
            disp_str.Get(display_item) = str;
    }
}

void CSettingsHelper::SaveDisplayStr(const wchar_t* AppName, const DispStrings& disp_str)
{
    for (const auto& item : disp_str.GetAllItems())
    {
        if (!item.first.IsPlugin())
        {
            WriteString(AppName, item.first.GetItemIniKeyName(), item.second);
        }
    }
}

void CSettingsHelper::LoadPluginDisplayStr(const wchar_t* AppName, DispStrings& disp_str, bool is_main_window)
{
    for (const auto& plugin : theApp.m_plugins.GetPluginItems())
    {
        std::wstring str{ plugin->GetItemLableText() };
        bool exist = GetString(AppName, plugin->GetItemId(), str);
        if (!is_main_window && AIUsageLayout::ColumnCount(plugin->GetItemId()))
        {
            if (std::wstring(plugin->GetItemId()) == L"AIUsageAntigravitySummary" && (str == L"AG:" || str == L"AG-G:" || str == L"AG-G"))
                str = L"AG";
            else if (!str.empty() && str.back() == L':')
                str.pop_back();
        }
        //主窗口只读取配置文件中存在的项，任务栏窗口读取所有项
        if (!is_main_window || exist)
            disp_str.Load(plugin->GetItemId(), str);
    }
}

void CSettingsHelper::SavePluginDisplayStr(const wchar_t* AppName, const DispStrings& disp_str)
{
    for (const auto& plugin : theApp.m_plugins.GetPluginItems())
    {
        if (disp_str.GetAllItems().find(plugin) != disp_str.GetAllItems().end())
            WriteString(AppName, plugin->GetItemId(), disp_str.GetConst(plugin));
    }
}
