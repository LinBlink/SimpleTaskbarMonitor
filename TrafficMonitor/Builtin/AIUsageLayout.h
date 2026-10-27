#pragma once
#include <algorithm>
#include <array>
#include <string>

namespace AIUsageLayout {
inline int ColumnCount(const std::wstring& id) {
    if (id == L"AIUsageCursorSummary") return 2;
    if (id == L"AIUsageMiniMaxSummary" || id == L"AIUsageCodexSummary" || id == L"AIUsageClaudeSummary") return 4;
    return 0;
}
struct Column { int left{}, width{}, marker{}, separator{}; };
struct Layout { std::array<Column,4> columns{}; int count{}, width{}; };
// Reserve the stale marker even for fresh data, so it never moves numbers.
template<class Measure> Layout MeasureColumns(int count, Measure measure, int gap) {
    Layout result; result.count=count;
    int digit=0;
    for(wchar_t c=L'0';c<=L'9';++c) digit=(std::max)(digit,measure(std::wstring(1,c)));
    for(int i=0;i<count;++i) {
        auto& col=result.columns[i]; col.left=result.width;
        col.width=(std::max)(measure(L"--"), i%2==0 ? digit*3+measure(L"%")
            : digit*(i==3?4:3)+measure(i==1 && count==2 ? L".d" : L".h"));
        col.marker=measure(L"*");
        col.separator=i+1<count ? measure(L",")+gap : 0;
        result.width+=col.width+col.marker+col.separator;
    }
    return result;
}
struct Value { std::wstring text; bool stale{}; };
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
