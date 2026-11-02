#include <windows.h>
#include <cassert>
#include <set>
#include <iostream>
#include "../SimpleTaskbarMonitor/Builtin/BuiltinFeatures.h"
#include "../SimpleTaskbarMonitor/Builtin/PowerValues.h"
#include "../SimpleTaskbarMonitor/Builtin/DateMath.h"
#include "../SimpleTaskbarMonitor/Builtin/ResetHours.h"
#include "../SimpleTaskbarMonitor/Builtin/GroupedUsage.h"
#include "../SimpleTaskbarMonitor/Builtin/ResetAlert.h"
#include "../SimpleTaskbarMonitor/Builtin/AIUsageLayout.h"
int main() {
    auto measure=[](const std::wstring& text) { return static_cast<int>(text.size()*8); };
    auto layout=AIUsageLayout::MeasureColumns(4,measure,2);
    assert(AIUsageLayout::ColumnCount(L"AIUsageCodexSummary")==4);
    assert(AIUsageLayout::ColumnCount(L"AIUsageCursorSummary")==2);
    assert(AIUsageLayout::ColumnCount(L"AIUsageAntigravitySummary")==4);
    assert(AIUsageLayout::ColumnCount(L"AIUsageAntigravityClaudeSummary")==4);
    assert(AIUsageLayout::ColumnCount(L"AIUsageAntigravityArgonSummary")==2);
    assert(AIUsageLayout::ColumnCount(L"AIUsageKimiSummary")==4);
    assert(AIUsageLayout::ColumnCount(L"BatteryPowerMon")==0);
    assert(layout.columns[0].left==layout.columns[2].left);
    assert(layout.columns[1].left==layout.columns[3].left);
    assert(layout.columns[0].row==0 && layout.columns[2].row==1);
    assert(layout.columns[1].width >= measure(L"168.0h"));
    assert(layout.columns[0].left+layout.columns[0].width < layout.columns[1].left);
    assert(layout.width < measure(L"100%*,5.0h*,100%*,168.0h*")*0.6);
    auto cursorLayout=AIUsageLayout::MeasureColumns(2,measure,2);
    assert(cursorLayout.columns[0].row==0 && cursorLayout.columns[1].row==1);
    assert(cursorLayout.width>=measure(L"31.0d") && cursorLayout.width<layout.width);
    auto fields=AIUsageLayout::Split(L"17%,3.1h,54%*,100.1h*");
    assert(fields[0].text==L"17%" && !fields[0].stale);
    assert(fields[2].text==L"54%" && fields[2].stale);
    assert(fields[3].text==L"100.1h" && fields[3].stale);
    fields=AIUsageLayout::Split(L"--,--,--,--");
    for(auto& field:fields) assert(field.text==L"--" && !field.stale);
    Builtin::ResetAlert alert, other;
    alert.Observe(90,100,0); // Expired startup cache: no alert.
    assert(!alert.Highlight(0));
    alert.Observe(110,100,10);
    assert(!alert.Highlight(10));
    alert.Observe(200,110,1000); // Refresher already advanced to the next window.
    assert(alert.Highlight(1000));
    assert(!alert.Highlight(1500));
    assert(alert.Highlight(2000));
    assert(!other.Highlight(2000)); // Providers have independent state.
    assert(!alert.Highlight(31000));
    alert.Observe(110,111,32000); // Repeated stale deadline must not re-trigger.
    assert(!alert.Highlight(32000));
    alert.Observe(200,200,40000);
    assert(alert.Highlight(40000)); // The next 5h cycle can alert again.
    Builtin::ResetAlert corrected;
    corrected.Observe(110,100,0);
    corrected.Observe(120,105,1); // Future deadline correction is not a reset.
    corrected.Observe(120,110,2);
    assert(!corrected.Highlight(2));
    corrected.Observe(0,120,3); // A temporary query failure preserves the armed deadline.
    assert(corrected.Highlight(3));
    const auto directory=Builtin::DataDirectory();
    // Isolated beside this test executable; never use installed account settings.
    WritePrivateProfileStringW(L"settings",L"EnableQueries",L"0",(directory+L"\\AIUsage.ini").c_str());
    FILETIME ft{};GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER t{};t.LowPart=ft.dwLowDateTime;t.HighPart=ft.dwHighDateTime;
    auto now=t.QuadPart/10000000ULL-11644473600ULL;
    const auto cache=directory+L"\\AIUsage.cache.ini";
    WritePrivateProfileStringW(L"codex5h",L"value",L"0%",cache.c_str());
    WritePrivateProfileStringW(L"codex5h",L"status",L"OK",cache.c_str());
    WritePrivateProfileStringW(L"codex5h",L"updated",std::to_wstring(now).c_str(),cache.c_str());
    WritePrivateProfileStringW(L"codex5h",L"resetEpoch",L"0",cache.c_str());
    WritePrivateProfileStringW(L"codexWeek",nullptr,nullptr,cache.c_str());
    auto features=Builtin::Features();assert(features.size()==4);
    std::set<std::wstring> ids;
    for(auto feature:features) for(int i=0;auto item=feature->GetItem(i);++i) assert(ids.insert(item->GetItemId()).second);
    assert(!Builtin::IsAIResetHighlighted(nullptr,GetTickCount64()));
    assert(!Builtin::IsAIResetHighlighted(Builtin::Power()->GetItem(0),GetTickCount64()));
    assert(Builtin::AIUsage()->GetItem(1)->IsDoubleLineExclusive());
    assert(ids.size()==15);assert(ids.count(L"AIUsageCodexSummary"));assert(ids.count(L"AIUsageKimiSummary"));assert(ids.count(L"AIUsageAntigravitySummary"));assert(ids.count(L"AIUsageAntigravityClaudeSummary"));assert(ids.count(L"AIUsageAntigravityArgonSummary"));assert(ids.count(L"CustomDualCountdown"));assert(ids.count(L"b5R30ITQ"));assert(ids.count(L"BatteryPowerMon"));
    for(auto feature:features) feature->DataRequired();
    assert(std::wstring(Builtin::AIUsage()->GetItem(1)->GetItemValueText())==L"0%,--,--,--");
    WritePrivateProfileStringW(L"codex5h",L"status",L"network unavailable",cache.c_str());
    Builtin::AIUsage()->DataRequired();assert(std::wstring(Builtin::AIUsage()->GetItem(1)->GetItemValueText())==L"0%*,--,--,--");
    GroupedUsage::Window shortWindow{L"17%",L"OK",now+11160,now,600};
    GroupedUsage::Window week{L"54%",L"OK",now+360360,now,600};
    assert(L"CX:"+GroupedUsage::Summary(shortWindow,week,now)==L"CX:17%,3.1h,54%,100.1h");
    WritePrivateProfileStringW(L"codex5h",L"value",L"17%",cache.c_str());
    WritePrivateProfileStringW(L"codex5h",L"status",L"OK",cache.c_str());
    WritePrivateProfileStringW(L"codex5h",L"resetEpoch",std::to_wstring(now+11160).c_str(),cache.c_str());
    WritePrivateProfileStringW(L"codexWeek",L"value",L"54%",cache.c_str());
    WritePrivateProfileStringW(L"codexWeek",L"status",L"OK",cache.c_str());
    WritePrivateProfileStringW(L"codexWeek",L"updated",std::to_wstring(now).c_str(),cache.c_str());
    WritePrivateProfileStringW(L"codexWeek",L"resetEpoch",std::to_wstring(now+360360).c_str(),cache.c_str());
    Builtin::AIUsage()->DataRequired();
    auto codex=Builtin::AIUsage()->GetItem(1);
    assert(std::wstring(codex->GetItemLableText())+codex->GetItemValueText()==L"CX:17%,3.1h,54%,100.1h");
    shortWindow.status=L"offline";
    assert(GroupedUsage::Summary(shortWindow,week,now)==L"17%*,3.1h*,54%,100.1h");
    shortWindow.status=L"OK";shortWindow.reset=now;
    assert(GroupedUsage::Reset(shortWindow,now)==L"0.0h*");
    assert(GroupedUsage::SummaryId(L"AIUsageCodexWeekReset")==L"AIUsageCodexSummary");
    assert(GroupedUsage::SummaryId(L"CustomDualCountdown")==L"CustomDualCountdown");
    assert(std::wstring(Builtin::DualCountdown()->GetItem(0)->GetItemValueText()).find(L" / ")!=std::wstring::npos);
    SYSTEMTIME day{},target{};
    assert(Countdown::ParseDate(L"2026-10-27",day));
    assert(Countdown::DayText(L"2026-10-27",day)==L"0d");assert(Countdown::DayText(L"2026-10-26",day)==L"-1d");
    assert(!Countdown::ParseDate(L"2026-02-29",target));assert(!Countdown::ParseDate(L"1900-02-29",target));assert(Countdown::ParseDate(L"2000-02-29",target));
    assert(Countdown::ParseDate(L"2028-02-28",day));assert(Countdown::DayText(L"2028-03-01",day)==L"2d");
    assert(ResetHours(3600,0)==L"1.0h");assert(ResetDays(86400,0)==L"1.0d");
    Builtin::PowerSnapshot state;assert(Builtin::Percentage(state)==L"--");
    state.present=true;state.percent=0;assert(Builtin::Percentage(state)==L"0%");
    state.percent=255;assert(Builtin::Percentage(state)==L"--");
    assert(Builtin::Measurement(NAN,L"W")==L"--");assert(Builtin::Measurement(-12.5,L"W")==L"-12.5W");assert(Builtin::Measurement(0,L"W")==L"0.0W");
    // Read-only probe of Windows battery APIs; unsupported values must stay unknown.
    Builtin::SampleHardware();Builtin::Battery()->DataRequired();Builtin::Power()->DataRequired();
    std::cout<<"PASS: 4 native features, 15 items, grouped quota/reset format, stale/zero AI quota, calendar boundaries, unknown battery/power\n";
}
