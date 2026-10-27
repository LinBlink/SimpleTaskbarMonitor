#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <algorithm>
#include "BuiltinFeatures.h"
#include "ResetHours.h"
#include "GroupedUsage.h"

namespace {
static std::wstring directory() { return Builtin::DataDirectory(); }
static std::wstring read(const std::wstring& file, const wchar_t* section,
                         const wchar_t* key, const wchar_t* fallback=L"") {
    wchar_t buf[2048]{};
    GetPrivateProfileStringW(section, key, fallback, buf, 2048, file.c_str());
    return buf;
}
class Item : public IPluginItem {
public:
    const wchar_t *name, *id, *label;
    std::wstring value=L"--";
    Item(const wchar_t* n,const wchar_t* i,const wchar_t* l):name(n),id(i),label(l){}
    const wchar_t* GetItemName() const override { return name; }
    const wchar_t* GetItemId() const override { return id; }
    const wchar_t* GetItemLableText() const override { return label; }
    const wchar_t* GetItemValueText() const override { return value.c_str(); }
    const wchar_t* GetItemValueSampleText() const override { return std::wstring(id)==L"AIUsageCursorSummary"?L"100%*,31.0d*":L"100%*,5.0h*,100%*,168.0h*"; }
};
class Plugin : public ITMPlugin {
    Item items[4]={{L"MiniMax 用量（5h/周）",L"AIUsageMiniMaxSummary",L"MM:"},
                   {L"Codex 用量（5h/周）",L"AIUsageCodexSummary",L"CX:"},
                   {L"Cursor 用量（月）",L"AIUsageCursorSummary",L"CU:"},
                   {L"Claude 用量（5h/周）",L"AIUsageClaudeSummary",L"CL:"}};
    HANDLE process=nullptr;
    HANDLE job=nullptr;
    ULONGLONG next=0,started=0;
    std::wstring tooltip;
public:
    ~Plugin() { if(job) CloseHandle(job); if(process) CloseHandle(process); }
    IPluginItem* GetItem(int i) override { return i>=0 && i<4 ? &items[i] : nullptr; }
    const wchar_t* GetInfo(PluginInfoIndex i) override {
        switch(i) {
        case TMI_NAME:return L"MiniMax / Codex / Cursor / Claude 用量";
        case TMI_DESCRIPTION:return L"订阅剩余额度，后台查询；* 表示数据过期。";
        case TMI_AUTHOR:return L"Local Usage Plugin";
        case TMI_VERSION:return L"2.0.0";
        default:return L"";
        }
    }
    void DataRequired() override {
        auto dir=directory(); auto cache=dir+L"\\AIUsage.cache.ini";
        auto now=GetTickCount64();
        if(process && (WaitForSingleObject(process,0)==WAIT_OBJECT_0 || now-started>120000)) {
            if(now-started>120000) { if(job) TerminateJobObject(job,1); else TerminateProcess(process,1); }
            CloseHandle(process); process=nullptr;
            if(job) { CloseHandle(job); job=nullptr; }
        }
        const wchar_t* keys[]={L"minimax5h",L"minimaxWeek",L"codex5h",L"codexWeek",L"cursorPlan",L"claude5h",L"claudeWeek"};
        const wchar_t* names[]={L"MiniMax 5h",L"MiniMax 周",L"Codex 5h",L"Codex 周",L"Cursor 月",L"Claude 5h",L"Claude 周"};
        FILETIME ft{};GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;
        const auto epoch=ticks.QuadPart/10000000ULL-11644473600ULL;
        GroupedUsage::Window windows[7];
        tooltip=L"剩余额度，重置倒计时（* = 旧值；-- = 未知）\n";
        for(int i=0;i<7;++i) {
            auto& window=windows[i];
            window.remaining=read(cache,keys[i],L"value",L"--");
            window.status=read(cache,keys[i],L"status",L"等待首次查询");
            window.updated=_wcstoui64(read(cache,keys[i],L"updated",L"0").c_str(),nullptr,10);
            window.reset=_wcstoui64(read(cache,keys[i],L"resetEpoch",L"0").c_str(),nullptr,10);
            window.maxAge=i>=5?1500:600;
            tooltip+=std::wstring(names[i])+L": "+GroupedUsage::Percent(window,epoch)+L"，重置剩余 "+
                GroupedUsage::Reset(window,epoch,i==4)+L"  "+window.status+L"\n";
        }
        items[0].value=GroupedUsage::Summary(windows[0],windows[1],epoch);
        items[1].value=GroupedUsage::Summary(windows[2],windows[3],epoch);
        items[2].value=GroupedUsage::Percent(windows[4],epoch)+L","+GroupedUsage::Reset(windows[4],epoch,true);
        items[3].value=GroupedUsage::Summary(windows[5],windows[6],epoch);
        if(!process && now>=next && GetPrivateProfileIntW(L"settings",L"EnableQueries",1,(dir+L"\\AIUsage.ini").c_str()) && GetFileAttributesW((dir+L"\\AIUsage.Refresh.ps1").c_str())!=INVALID_FILE_ATTRIBUTES) {
            int seconds=GetPrivateProfileIntW(L"settings",L"RefreshSeconds",180,(dir+L"\\AIUsage.ini").c_str());
            next=now+static_cast<ULONGLONG>(std::clamp(seconds,60,3600))*1000;
            wchar_t system[32768]; GetSystemDirectoryW(system,32768);
            auto exe=std::wstring(system)+L"\\WindowsPowerShell\\v1.0\\powershell.exe";
            auto cmd=L"\""+exe+L"\" -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File \""+dir+L"\\AIUsage.Refresh.ps1\"";
            STARTUPINFOW si{};si.cb=sizeof(si); PROCESS_INFORMATION pi{};
            job=CreateJobObjectW(nullptr,nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(job) SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
            if(CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,dir.c_str(),&si,&pi)) {
                if(!job || !AssignProcessToJobObject(job,pi.hProcess)) {
                    TerminateProcess(pi.hProcess,1);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
                    if(job) {CloseHandle(job);job=nullptr;} tooltip+=L"无法建立后台查询进程组。\n";
                } else {process=pi.hProcess;ResumeThread(pi.hThread);CloseHandle(pi.hThread);started=now;}
            } else tooltip+=L"无法启动后台查询脚本。\n";
        }
    }
    const wchar_t* GetTooltipInfo() override { return tooltip.c_str(); }
    OptionReturn ShowOptionsDialog(void*) override {
        ShellExecuteW(nullptr,L"open",(directory()+L"\\AIUsage.Setup.cmd").c_str(),nullptr,directory().c_str(),SW_SHOWNORMAL);
        return OR_OPTION_UNCHANGED;
    }
    int GetCommandCount() override { return 1; }
    const wchar_t* GetCommandName(int) override { return L"刷新订阅额度"; }
    void OnPluginCommand(int,void*,void*) override { next=0; }
};
}
ITMPlugin* Builtin::AIUsage() { static Plugin feature; return &feature; }
