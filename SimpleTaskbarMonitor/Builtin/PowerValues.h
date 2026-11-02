#pragma once
#include <string>
#include <cwchar>
#include <cmath>
namespace Builtin {
struct PowerSnapshot {
    bool present{};
    int percent{-1};
    int ac{-1};
    bool charging{};
    double watts{NAN}, wattHours{NAN}, volts{NAN}, hours{NAN};
};
inline std::wstring Measurement(double value, const wchar_t* unit) {
    if (!std::isfinite(value)) return L"--";
    wchar_t text[64]{};
    swprintf_s(text, L"%.1f%s", value, unit);
    return text;
}
inline std::wstring Percentage(const PowerSnapshot& state) {
    return state.present && state.percent >= 0 && state.percent <= 100
        ? std::to_wstring(state.percent) + L"%" : L"--";
}
PowerSnapshot GetPowerSnapshot();
}
