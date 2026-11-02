#pragma once
#include <string>
#include "ResetHours.h"
namespace GroupedUsage {
struct Window {
    std::wstring remaining{L"--"}, status;
    unsigned long long reset{}, updated{}, maxAge{600};
};
inline bool Stale(const Window& w,unsigned long long now) {
    return !w.updated || now>w.updated+w.maxAge || w.status!=L"OK";
}
inline std::wstring Percent(const Window& w,unsigned long long now) {
    return w.remaining.empty() || w.remaining==L"--" ? L"--" : w.remaining+(Stale(w,now)?L"*":L"");
}
inline std::wstring Reset(const Window& w,unsigned long long now,bool days=false) {
    if(!w.reset) return L"--";
    return (days?ResetDays(w.reset,now):ResetHours(w.reset,now)) + ((Stale(w,now)||now>=w.reset)?L"*":L"");
}
inline std::wstring Summary(const Window& shortWindow,const Window& week,unsigned long long now) {
    return Percent(shortWindow,now)+L","+Reset(shortWindow,now)+L","+Percent(week,now)+L","+Reset(week,now);
}
inline std::wstring SummaryId(const std::wstring& id) {
    const wchar_t* providers[]={L"MiniMax",L"Codex",L"Cursor",L"Claude"};
    for(auto provider:providers) {
        const auto prefix=std::wstring(L"AIUsage")+provider;
        if(id.compare(0,prefix.size(),prefix)==0) return prefix+L"Summary";
    }
    return id;
}
inline std::wstring LegacyColorId(const std::wstring& id) {
    if(id==L"AIUsageMiniMaxSummary")return L"AIUsageMiniMax5h";
    if(id==L"AIUsageCodexSummary")return L"AIUsageCodex5h";
    if(id==L"AIUsageClaudeSummary")return L"AIUsageClaude5h";
    if(id==L"AIUsageCursorSummary")return L"AIUsageCursorPlan";
    return id;
}
}
