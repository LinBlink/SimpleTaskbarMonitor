#pragma once
#include <string>
#include <vector>
#include <map>
#include <cwchar>
namespace StableItemColors {
struct Pair { unsigned long label{}, value{}; };
inline unsigned long Number(const std::wstring& text, unsigned long fallback) {
    wchar_t* end = nullptr;
    auto value = wcstoul(text.c_str(), &end, 10);
    return end != text.c_str() && *end == 0 && value <= 0xffffff ? value : fallback;
}
inline std::vector<Pair> Decode(const std::vector<std::wstring>& numbers,
    const std::vector<std::wstring>& savedIds, const std::vector<std::wstring>& currentIds,
    unsigned long fallback) {
    std::map<std::wstring, Pair> colors;
    const auto& ids = savedIds.empty() ? currentIds : savedIds;
    for (size_t i=0; i<ids.size() && i*2<numbers.size(); ++i)
        colors[ids[i]] = {Number(numbers[i*2],fallback), i*2+1<numbers.size()?Number(numbers[i*2+1],fallback):fallback};
    std::vector<Pair> result;
    for (const auto& id : currentIds) {
        auto it=colors.find(id);
        result.push_back(it==colors.end()?Pair{fallback,fallback}:it->second);
    }
    return result;
}
}
