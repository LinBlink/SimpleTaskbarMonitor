#pragma once
#include <algorithm>
#include <array>
#include <string>

namespace AIUsageLayout {
inline int ColumnCount(const std::wstring& id) {
    if (id == L"AIUsageCursorSummary" || id == L"AIUsageAntigravitySummary") return 2;
    if (id == L"AIUsageMiniMaxSummary" || id == L"AIUsageCodexSummary" || id == L"AIUsageClaudeSummary" || id == L"AIUsageKimiSummary") return 4;
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
}
