#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <cmath>
#include "Temperature.h"
namespace {
class TemperatureMonitor {
    HANDLE process{},job{},stop{};
    unsigned int flags{};
    std::wstring core;
    ULONGLONG retryAfter{};
    void Stop() {
        if(stop) SetEvent(stop);
        if(process && stop) WaitForSingleObject(process,3000);
        if(job) CloseHandle(job);
        if(process) CloseHandle(process);
        if(stop) CloseHandle(stop);
        process=job=stop=nullptr;
    }
    static float Read(const std::wstring& file,const wchar_t* key) {
        wchar_t value[64]{};
        GetPrivateProfileStringW(L"temperature",key,L"-1",value,64,file.c_str());
        wchar_t* end=nullptr;float result=wcstof(value,&end);
        return end!=value && *end==0 && std::isfinite(result) && result>=0 && result<=150 ? result : -1;
    }
public:
    ~TemperatureMonitor(){Stop();}
    void Sample(unsigned int enabled,const std::wstring& cpuCore,float& cpu,float& gpu) {
        cpu=gpu=-1;
        enabled&=3;
        if(flags!=enabled || core!=cpuCore) {Stop();flags=enabled;core=cpuCore;retryAfter=0;}
        if(!enabled){Stop();return;}
        wchar_t module[32768]{};GetModuleFileNameW(nullptr,module,32768);
        std::wstring directory(module);directory.resize(directory.find_last_of(L"\\/"));
        const auto file=directory+LR"(\temperature.cache.ini)";
        const auto executable=directory+LR"(\sensors\TemperatureWorker.exe)";
        if(process && WaitForSingleObject(process,0)==WAIT_OBJECT_0) {Stop();retryAfter=GetTickCount64()+30000;}
        if(!process && GetTickCount64()>=retryAfter && GetFileAttributesW(executable.c_str())!=INVALID_FILE_ATTRIBUTES) {
            // Quote the user-selected sensor name as a single Windows argument.
            std::wstring safeCore;
            for(wchar_t c:cpuCore) if(c!=34 && c!=92) safeCore+=c;
            auto command=L"\""+executable+L"\" \""+file+L"\" "+std::to_wstring(GetCurrentProcessId())+
                ((enabled&1)?L" 1":L" 0")+((enabled&2)?L" 1":L" 0")+L" \""+safeCore+L"\"";
            auto eventName=L"Local\\SimpleTaskbarMonitor.Sensors.Stop."+std::to_wstring(GetCurrentProcessId());
            stop=CreateEventW(nullptr,TRUE,FALSE,eventName.c_str());
            if(stop) ResetEvent(stop);
            STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION child{};
            job=CreateJobObjectW(nullptr,nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(job) SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits));
            if(CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW|CREATE_SUSPENDED,nullptr,directory.c_str(),&startup,&child)) {
                if(job && AssignProcessToJobObject(job,child.hProcess)) {process=child.hProcess;ResumeThread(child.hThread);}
                else {TerminateProcess(child.hProcess,1);CloseHandle(child.hProcess);}
                CloseHandle(child.hThread);
            }
            if(!process) {Stop();retryAfter=GetTickCount64()+30000;}
        }
        if(!process) return;
        wchar_t updated[64]{};GetPrivateProfileStringW(L"temperature",L"updated",L"0",updated,64,file.c_str());
        auto stamp=_wcstoui64(updated,nullptr,10);
        FILETIME ft{};GetSystemTimeAsFileTime(&ft);ULARGE_INTEGER ticks{};ticks.LowPart=ft.dwLowDateTime;ticks.HighPart=ft.dwHighDateTime;
        auto now=ticks.QuadPart/10000000ULL-11644473600ULL;
        if(!stamp || stamp>now+5 || now>stamp+12) return;
        if(enabled&1) cpu=Read(file,L"cpu");
        if(enabled&2) gpu=Read(file,L"gpu");
    }
};
}
void Builtin::SampleTemperatures(unsigned int enabled,const std::wstring& cpuCore,float& cpu,float& gpu) {
    static TemperatureMonitor monitor;monitor.Sample(enabled,cpuCore,cpu,gpu);
}
