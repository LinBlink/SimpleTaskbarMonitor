#pragma once
#include <windows.h>
#include <string>
namespace Countdown {
inline bool ParseDate(const std::wstring& text, SYSTEMTIME& date) {
    if(text.size()!=10 || text[4]!=L'-' || text[7]!=L'-') return false;
    for(int i=0;i<10;i++) if(i!=4 && i!=7 && (text[i]<L'0' || text[i]>L'9')) return false;
    int y=std::stoi(text.substr(0,4)),m=std::stoi(text.substr(5,2)),d=std::stoi(text.substr(8,2));
    if(y<1601 || y>9999 || m<1 || m>12 || d<1) return false;
    const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    int max=days[m-1]+(m==2 && y%4==0 && (y%100!=0 || y%400==0));
    if(d>max)return false;
    date={};date.wYear=static_cast<WORD>(y);date.wMonth=static_cast<WORD>(m);date.wDay=static_cast<WORD>(d);
    return true;
}
inline long long SerialDay(SYSTEMTIME date) {
    date.wHour=date.wMinute=date.wSecond=date.wMilliseconds=0;
    FILETIME ft{};SystemTimeToFileTime(&date,&ft);
    ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;
    return static_cast<long long>(ticks.QuadPart/864000000000ULL);
}
inline long long DaysUntil(const SYSTEMTIME& target,const SYSTEMTIME& today) {
    // Compare calendar dates as midnight values, independent of DST and time of day.
    return SerialDay(target)-SerialDay(today);
}
inline std::wstring DayText(const std::wstring& date,const SYSTEMTIME& today) {
    if(date.empty())return L"--";
    SYSTEMTIME target{};
    if(!ParseDate(date,target))return L"Err";
    return std::to_wstring(DaysUntil(target,today))+L"d";
}
}
