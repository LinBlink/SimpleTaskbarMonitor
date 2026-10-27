#include <windows.h>
#include <cassert>
#include <set>
#include <iostream>
#include "../TrafficMonitor/Builtin/BuiltinFeatures.h"
#include "../TrafficMonitor/Builtin/PowerValues.h"
#include "../TrafficMonitor/Builtin/DateMath.h"
#include "../TrafficMonitor/Builtin/ResetHours.h"
#include "../TrafficMonitor/Builtin/GroupedUsage.h"
int main() {
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
    assert(ids.size()==11);assert(ids.count(L"AIUsageCodexSummary"));assert(ids.count(L"CustomDualCountdown"));assert(ids.count(L"b5R30ITQ"));assert(ids.count(L"BatteryPowerMon"));
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
    std::cout<<"PASS: 4 native features, 11 items, grouped quota/reset format, stale/zero AI quota, calendar boundaries, unknown battery/power\n";
}
