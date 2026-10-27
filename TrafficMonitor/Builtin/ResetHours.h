#pragma once
#include <string>
#include <cwchar>
inline std::wstring ResetHours(unsigned long long reset, unsigned long long now) {
    if(!reset) return L"--";
    double hours=reset>now?static_cast<double>(reset-now)/3600.0:0.0;
    wchar_t buffer[64];
    swprintf_s(buffer,L"%.1fh",hours);
    return buffer;
}
inline std::wstring ResetDays(unsigned long long reset, unsigned long long now) {
    if(!reset) return L"--";
    double days=reset>now?static_cast<double>(reset-now)/86400.0:0.0;
    wchar_t buffer[64];
    swprintf_s(buffer,L"%.1fd",days);
    return buffer;
}
