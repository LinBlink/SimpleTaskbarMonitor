#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <string>
#include <algorithm>
#include "BuiltinFeatures.h"
#include "ResetHours.h"
#include "GroupedUsage.h"
#include "ResetAlert.h"

namespace {
static std::wstring directory() { return Builtin::DataDirectory(); }
static std::wstring read(const std::wstring& file, const wchar_t* section,
                         const wchar_t* key, const wchar_t* fallback=L"") {
    wchar_t buf[2048]{};
    GetPrivateProfileStringW(section, key, fallback, buf, 2048, file.c_str());
    return buf;
}
static std::wstring stripPct(const std::wstring& s) {
    if (s.empty() || s == L"--") return L"--";
    auto out = s;
    if (!out.empty() && out.back() == L'*') out.pop_back();
    if (!out.empty() && out.back() == L'%') out.pop_back();
    return out.empty() ? L"--" : out;
}
static std::wstring agSummary(const GroupedUsage::Window& g5, const GroupedUsage::Window& gw,
                              const GroupedUsage::Window& c5, const GroupedUsage::Window& cw,
                              const GroupedUsage::Window& aw, const GroupedUsage::Window& plan,
                              unsigned long long epoch) {
    const bool hasGroups = g5.remaining != L"--" || gw.remaining != L"--" ||
                           c5.remaining != L"--" || cw.remaining != L"--" || aw.remaining != L"--";
    if (!hasGroups) {
        auto p = GroupedUsage::Percent(plan, epoch);
        auto r = GroupedUsage::Reset(plan, epoch, false);
        return p + L"," + r + L"," + p + L"," + r;
    }
    const bool topStale = (g5.remaining != L"--" && GroupedUsage::Stale(g5, epoch)) ||
                          (c5.remaining != L"--" && GroupedUsage::Stale(c5, epoch)) ||
                          (aw.remaining != L"--" && GroupedUsage::Stale(aw, epoch));
    const bool botStale = (gw.remaining != L"--" && GroupedUsage::Stale(gw, epoch)) ||
                          (cw.remaining != L"--" && GroupedUsage::Stale(cw, epoch)) ||
                          (aw.remaining != L"--" && GroupedUsage::Stale(aw, epoch));
    std::wstring topPct = stripPct(g5.remaining) + L"/" + stripPct(c5.remaining) + L"/" + stripPct(aw.remaining) + L"%" + (topStale ? L"*" : L"");
    std::wstring botPct = stripPct(gw.remaining) + L"/" + stripPct(cw.remaining) + L"/" + stripPct(aw.remaining) + L"%" + (botStale ? L"*" : L"");

    const GroupedUsage::Window* shortWin = nullptr;
    if (g5.reset > epoch) shortWin = &g5;
    if (c5.reset > epoch && (!shortWin || c5.reset < shortWin->reset)) shortWin = &c5;

    const GroupedUsage::Window* weekWin = nullptr;
    const GroupedUsage::Window* weeks[] = { &gw, &cw, &aw };
    for (auto w : weeks) {
        if (w->reset > 0 && (!weekWin || w->reset < weekWin->reset))
            weekWin = w;
    }
    if (!shortWin) shortWin = weekWin;

    std::wstring shortReset = shortWin ? GroupedUsage::Reset(*shortWin, epoch, false) : L"--";
    std::wstring weekReset = weekWin ? GroupedUsage::Reset(*weekWin, epoch, false) : L"--";
    return topPct + L"," + shortReset + L"," + botPct + L"," + weekReset;
}
class Item : public IPluginItem {
public:
    const wchar_t *name, *id, *label;
    std::wstring value=L"--";
    Builtin::ResetAlert resetAlert;
    Item(const wchar_t* n,const wchar_t* i,const wchar_t* l):name(n),id(i),label(l){}
    int IsDoubleLineExclusive() const override { return 1; }
    const wchar_t* GetItemName() const override { return name; }
    const wchar_t* GetItemId() const override { return id; }
    const wchar_t* GetItemLableText() const override { return label; }
    const wchar_t* GetItemValueText() const override { return value.c_str(); }
    const wchar_t* GetItemValueSampleText() const override {
        const std::wstring itemId=id;
        return itemId==L"AIUsageCursorSummary" ? L"100%*,31.0d*" : L"100%*,5.0h*,100%*,168.0h*";
    }
};
class Plugin : public ITMPlugin {
    Item items[6]={{L"MiniMax usage (5h/week)",L"AIUsageMiniMaxSummary",L"MM"},
                   {L"Codex usage (5h/week)",L"AIUsageCodexSummary",L"CX"},
                   {L"Cursor usage (month)",L"AIUsageCursorSummary",L"CU"},
                   {L"Claude usage (5h/week)",L"AIUsageClaudeSummary",L"CL"},
                   {L"Kimi usage (5h/second period)",L"AIUsageKimiSummary",L"KI"},
                   {L"Google Antigravity usage (G/C/A)",L"AIUsageAntigravitySummary",L"AG"}};
    HANDLE process=nullptr;
    HANDLE job=nullptr;
    ULONGLONG next=0,started=0;
    std::wstring tooltip;
public:
    ~Plugin() { if(job) CloseHandle(job); if(process) CloseHandle(process); }
    IPluginItem* GetItem(int i) override { return i>=0 && i<6 ? &items[i] : nullptr; }
    const wchar_t* GetInfo(PluginInfoIndex i) override {
        switch(i) {
        case TMI_NAME:return L"AI subscription usage";
        case TMI_DESCRIPTION:return L"Remaining quota from MiniMax, Codex, Cursor, Claude, Kimi and Google Antigravity. * marks stale data.";
        case TMI_AUTHOR:return L"Local Usage Plugin";
        case TMI_VERSION:return L"3.1.0";
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
        const wchar_t* keys[]={L"minimax5h",L"minimaxWeek",L"codex5h",L"codexWeek",L"cursorPlan",L"claude5h",L"claudeWeek",L"kimi5h",L"kimiWeek",L"antigravityGemini5h",L"antigravityGeminiWeek",L"antigravityClaude5h",L"antigravityClaudeWeek",L"antigravityArgonWeek",L"antigravityPlan"};
        const wchar_t* names[]={L"MiniMax 5h",L"MiniMax week",L"Codex 5h",L"Codex week",L"Cursor month",L"Claude 5h",L"Claude week",L"Kimi 5h",L"Kimi month",L"Antigravity Gemini 5h",L"Antigravity Gemini week",L"Antigravity Claude/GPT 5h",L"Antigravity Claude/GPT week",L"Antigravity Argon week",L"Antigravity overall"};
        FILETIME ft{};GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;
        const auto epoch=ticks.QuadPart/10000000ULL-11644473600ULL;
        GroupedUsage::Window windows[15];
        tooltip=L"Compact 2x2 AI usage with bottom weekly/monthly reset time bar.\n";
        for(int i=0;i<15;++i) {
            auto& window=windows[i];
            window.remaining=read(cache,keys[i],L"value",L"--");
            window.status=read(cache,keys[i],L"status",L"Waiting for first query");
            window.updated=_wcstoui64(read(cache,keys[i],L"updated",L"0").c_str(),nullptr,10);
            window.reset=_wcstoui64(read(cache,keys[i],L"resetEpoch",L"0").c_str(),nullptr,10);
            window.maxAge=i==5 || i==6?1500:600;
            if(i<14) {
                tooltip+=std::wstring(names[i])+L": "+GroupedUsage::Percent(window,epoch)+L", reset in "+
                    GroupedUsage::Reset(window,epoch,i==4)+L"  "+window.status+L"\n";
            }
        }
        items[0].resetAlert.Observe(windows[0].reset,epoch,now);
        items[1].resetAlert.Observe(windows[2].reset,epoch,now);
        items[3].resetAlert.Observe(windows[5].reset,epoch,now);
        items[4].resetAlert.Observe(windows[7].reset,epoch,now);
        unsigned long long agReset = windows[9].reset;
        if (windows[11].reset > 0 && (agReset == 0 || windows[11].reset < agReset)) agReset = windows[11].reset;
        if (agReset == 0) agReset = windows[14].reset;
        items[5].resetAlert.Observe(agReset,epoch,now);
        items[0].value=GroupedUsage::Summary(windows[0],windows[1],epoch);
        items[1].value=GroupedUsage::Summary(windows[2],windows[3],epoch);
        items[2].value=GroupedUsage::Percent(windows[4],epoch)+L","+GroupedUsage::Reset(windows[4],epoch,true);
        items[3].value=GroupedUsage::Summary(windows[5],windows[6],epoch);
        items[4].value=GroupedUsage::Summary(windows[7],windows[8],epoch);
        items[5].value=agSummary(windows[9],windows[10],windows[11],windows[12],windows[13],windows[14],epoch);
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
                    if(job) {CloseHandle(job);job=nullptr;} tooltip+=L"Cannot create the background query job.\n";
                } else {process=pi.hProcess;ResumeThread(pi.hThread);CloseHandle(pi.hThread);started=now;}
            } else tooltip+=L"Cannot start the background query script.\n";
        }
    }
    const wchar_t* GetTooltipInfo() override { return tooltip.c_str(); }
    OptionReturn ShowOptionsDialog(void*) override { return OR_OPTION_NOT_PROVIDED; }
    int GetCommandCount() override { return 1; }
    const wchar_t* GetCommandName(int) override { return L"Refresh subscription usage"; }
    void OnPluginCommand(int,void*,void*) override { next=0; }
};
}
ITMPlugin* Builtin::AIUsage() { static Plugin feature; return &feature; }
void Builtin::RequestAIUsageRefresh() { static_cast<Plugin*>(AIUsage())->OnPluginCommand(0, nullptr, nullptr); }

bool Builtin::IsAIResetHighlighted(const IPluginItem* item, unsigned long long tick) {
    auto feature=AIUsage();
    for(int i=0;i<6;++i)
        if(feature->GetItem(i)==item)
            return static_cast<const Item*>(item)->resetAlert.Highlight(tick);
    return false;
}
