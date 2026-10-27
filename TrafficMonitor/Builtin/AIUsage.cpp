#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <algorithm>
#include "BuiltinFeatures.h"
#include "ResetHours.h"

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
    bool resetTime=false;
    bool resetDays=false;
    Item(const wchar_t* n,const wchar_t* i,const wchar_t* l,bool r=false,bool d=false):name(n),id(i),label(l),resetTime(r),resetDays(d){}
    const wchar_t* GetItemName() const override { return name; }
    const wchar_t* GetItemId() const override { return id; }
    const wchar_t* GetItemLableText() const override { return label; }
    const wchar_t* GetItemValueText() const override { return value.c_str(); }
    const wchar_t* GetItemValueSampleText() const override { return resetDays?L"31.0d*":(resetTime?L"168.0h*":L"100%*"); }
};
class Plugin : public ITMPlugin {
    Item items[11]={{L"MiniMax 5小时剩余",L"AIUsageMiniMax5h",L"MM5h: "},
                   {L"MiniMax 每周剩余",L"AIUsageMiniMaxWeek",L"MM周: "},
                   {L"Codex 5小时剩余",L"AIUsageCodex5h",L"CX5h: "},
                   {L"Codex 每周剩余",L"AIUsageCodexWeek",L"CX周: "},
                   {L"MiniMax 周重置剩余小时",L"AIUsageMiniMaxWeekReset",L"MM重置: ",true},
                   {L"Codex 周重置剩余小时",L"AIUsageCodexWeekReset",L"CX重置: ",true},
                   {L"Cursor 总体剩余",L"AIUsageCursorPlan",L"CU: "},
                   {L"Cursor 月重置剩余天数",L"AIUsageCursorReset",L"CU重置: ",true,true},
                   {L"Claude 5小时剩余",L"AIUsageClaude5h",L"CL5h: "},
                   {L"Claude 每周剩余",L"AIUsageClaudeWeek",L"CL周: "},
                   {L"Claude 周重置剩余小时",L"AIUsageClaudeWeekReset",L"CL重置: ",true}};
    HANDLE process=nullptr;
    HANDLE job=nullptr;
    ULONGLONG next=0,started=0;
    std::wstring tooltip;
public:
    ~Plugin() { if(job) CloseHandle(job); if(process) CloseHandle(process); }
    IPluginItem* GetItem(int i) override { return i>=0 && i<11 ? &items[i] : nullptr; }
    const wchar_t* GetInfo(PluginInfoIndex i) override {
        switch(i) {
        case TMI_NAME:return L"MiniMax / Codex / Cursor / Claude 用量";
        case TMI_DESCRIPTION:return L"订阅剩余额度，后台查询；* 表示数据过期。";
        case TMI_AUTHOR:return L"Local Usage Plugin";
        case TMI_VERSION:return L"1.3.0";
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
        const int valueItems[]={0,1,2,3,6,8,9};
        const int resetItems[]={-1,4,-1,5,7,-1,10};
        tooltip=L"订阅剩余额度（* = 过期；-- = 暂无数据）\n";
        for(int i=0;i<7;i++) {
            auto stamp=read(cache,keys[i],L"updated",L"0");
            auto value=read(cache,keys[i],L"value",L"--");
            auto error=read(cache,keys[i],L"status",L"等待首次查询");
            FILETIME ft; GetSystemTimeAsFileTime(&ft);
            ULARGE_INTEGER t; t.LowPart=ft.dwLowDateTime;t.HighPart=ft.dwHighDateTime;
            auto epoch=t.QuadPart/10000000ULL-11644473600ULL;
            auto ts=_wcstoui64(stamp.c_str(),nullptr,10);
            auto limit=i>=5?1500ULL:600ULL;
            bool stale=value!=L"--" && (!ts || epoch>ts+limit || error!=L"OK");
            auto& item=items[valueItems[i]];
            item.value=value+(stale?L"*":L"");
            tooltip+=std::wstring(item.name)+L": "+item.value+L"  "+error;
            auto reset=read(cache,keys[i],L"reset");
            if(resetItems[i]>=0) {
                auto resetEpoch=_wcstoui64(read(cache,keys[i],L"resetEpoch",L"0").c_str(),nullptr,10);
                bool resetStale=resetEpoch && (!ts || epoch>ts+limit || error!=L"OK" || epoch>=resetEpoch);
                items[resetItems[i]].value=(i==4?ResetDays(resetEpoch,epoch):ResetHours(resetEpoch,epoch))+(resetStale?L"*":L"");
            }
            if(!reset.empty()) tooltip+=L"  重置: "+reset;
            tooltip+=L"\n";
        }
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
