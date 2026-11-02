#pragma once
#include <algorithm>
#include <array>
#include <cwchar>
#include <string>

namespace AIUsageLayout {
inline int ColumnCount(const std::wstring& id) {
    if (id == L"AIUsageCursorSummary") return 2;
    if (id == L"AIUsageMiniMaxSummary" || id == L"AIUsageCodexSummary" || id == L"AIUsageClaudeSummary" || id == L"AIUsageKimiSummary" || id == L"AIUsageAntigravitySummary") return 4;
    return 0;
}
struct Column { int left{}, width{}, row{}; };
struct Layout { std::array<Column,4> columns{}; int count{}, width{}; };
// Both quota periods share physical columns; stale indicators belong to the label.
template<class Measure> Layout MeasureColumns(int count, Measure measure, int gap) {
    Layout result; result.count=count;
    int digit=0;
    for(wchar_t c=L'0';c<=L'9';++c) digit=(std::max)(digit,measure(std::wstring(1,c)));
    const int percent=(std::max)(measure(L"--"),digit*3+measure(L"%"));
    const int time=(std::max)(measure(L"--"),digit*(count==2?3:4)+measure(count==2?L".d":L".h"));
    if(count==2) {
        result.width=(std::max)(percent,time);
        result.columns[0]={0,result.width,0};
        result.columns[1]={0,result.width,1};
    } else if(count==4) {
        result.width=percent+gap+time;
        for(int i=0;i<4;++i) result.columns[i]={i%2?percent+gap:0,i%2?time:percent,i/2};
    }
    return result;
}
struct Value { std::wstring text=L"--"; bool stale{}; };
inline std::array<Value,4> Split(const std::wstring& summary) {
    std::array<Value,4> result;
    size_t start=0;
    for(auto& field:result) {
        auto end=summary.find(L',',start);
        field.text=summary.substr(start,end==std::wstring::npos ? end : end-start);
        if(!field.text.empty() && field.text.back()==L'*') {field.stale=true;field.text.pop_back();}
        if(field.text.empty()) field.text=L"--";
        if(end==std::wstring::npos) break;
        start=end+1;
    }
    return result;
}
inline std::wstring CompactTime(const std::wstring& raw) {
    if (raw.empty() || raw == L"--") return L"--";
    wchar_t* end = nullptr;
    double val = std::wcstod(raw.c_str(), &end);
    if (end == raw.c_str() || !end || *end == 0) return raw;
    wchar_t unit = *end;
    wchar_t buf[32]{};
    if (unit == L'h' || unit == L'H') {
        if (val >= 48.0) {
            double days = val / 24.0;
            if (days >= 9.95) std::swprintf(buf, 32, L"%.0fd", days);
            else std::swprintf(buf, 32, L"%.1fd", days);
            return buf;
        }
        if (val >= 9.95) {
            std::swprintf(buf, 32, L"%.0fh", val);
            return buf;
        }
        std::swprintf(buf, 32, L"%.1fh", val);
        return buf;
    }
    if (unit == L'd' || unit == L'D') {
        if (val >= 9.95) std::swprintf(buf, 32, L"%.0fd", val);
        else std::swprintf(buf, 32, L"%.1fd", val);
        return buf;
    }
    return raw;
}
inline int ResetProgressPermille(const std::wstring& raw, double periodHours) {
    if (raw.empty() || raw == L"--" || periodHours <= 0.0) return -1;
    wchar_t* end = nullptr;
    double val = std::wcstod(raw.c_str(), &end);
    if (end == raw.c_str() || !end || *end == 0) return -1;
    double hours = (*end == L'd' || *end == L'D') ? val * 24.0 : val;
    double ratio = std::clamp(hours / periodHours, 0.0, 1.0);
    return static_cast<int>(ratio * 1000.0 + 0.5);
}
inline int MinQuotaPercent(const std::wstring& text) {
    if (text.empty() || text == L"--") return -1;
    int minPct = 101;
    const wchar_t* p = text.c_str();
    while (*p) {
        if (*p >= L'0' && *p <= L'9') {
            wchar_t* end = nullptr;
            long v = std::wcstol(p, &end, 10);
            if (end != p) {
                minPct = (std::min)(minPct, static_cast<int>(std::clamp(v, 0L, 100L)));
                p = end;
                continue;
            }
        }
        ++p;
    }
    return minPct <= 100 ? minPct : -1;
}
}
