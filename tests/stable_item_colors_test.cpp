#include "../SimpleTaskbarMonitor/StableItemColors.h"
#include <cassert>
#include <iostream>
int main() {
    using namespace StableItemColors;
    std::vector<std::wstring> values{L"1",L"2",L"3",L"4",L"5",L"6"};
    auto result=Decode(values,{L"cpu",L"gpu_temp",L"AIUsageCodex5h"},{L"AIUsageCodex5h",L"cpu",L"new_item",L"gpu_temp"},9);
    assert(result[0].label==5 && result[0].value==6);
    assert(result[1].label==1 && result[1].value==2);
    assert(result[2].label==9 && result[2].value==9);
    assert(result[3].label==3 && result[3].value==4);
    result=Decode(values,{}, {L"cpu",L"gpu_temp"},9);assert(result[1].label==3);
    result=Decode({L"0",L"16777215",L"garbage",L"16777216"},{L"cpu",L"bad"},{L"cpu",L"bad"},9);
    assert(result[0].label==0 && result[0].value==0xffffff);
    assert(result[1].label==9 && result[1].value==9);
    result=Decode({L"123",L"456"},{L"AIUsageCodex5h"},{L"AIUsageCodexSummary"},9);
    assert(result[0].label==123 && result[0].value==456);
    std::cout<<"PASS: colors follow stable IDs across insertions/reordering; legacy arrays and RGB bounds\n";
}
