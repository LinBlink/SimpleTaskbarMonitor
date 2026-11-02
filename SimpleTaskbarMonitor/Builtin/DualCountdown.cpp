#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include "BuiltinFeatures.h"
#include "DateMath.h"
#include "BuiltinResource.h"
namespace {
static std::wstring ConfigPath() { return Builtin::DataDirectory()+L"\\Countdown.ini"; }
static std::wstring Read(const wchar_t* key,const wchar_t* fallback) {
    wchar_t value[256]{};GetPrivateProfileStringW(L"countdown",key,fallback,value,256,ConfigPath().c_str());
    return value;
}
static std::wstring names[2],dates[2];
static void Load() {
    names[0]=Read(L"Name1",L"Date 1").substr(0,8); names[1]=Read(L"Name2",L"Date 2").substr(0,8);
    if (names[0] == L"\x65e5\x671f" L"1") names[0] = L"Date 1";
    if (names[1] == L"\x65e5\x671f" L"2") names[1] = L"Date 2";
    dates[0]=Read(L"Date1",L"");dates[1]=Read(L"Date2",L"");
}
static INT_PTR CALLBACK SettingsProc(HWND dialog,UINT message,WPARAM wParam,LPARAM) {
    if(message==WM_INITDIALOG) {
        SetDlgItemTextW(dialog,IDC_COUNTDOWN_NAME1,names[0].c_str());SetDlgItemTextW(dialog,IDC_COUNTDOWN_NAME2,names[1].c_str());
        SetDlgItemTextW(dialog,IDC_COUNTDOWN_DATE1,dates[0].c_str());SetDlgItemTextW(dialog,IDC_COUNTDOWN_DATE2,dates[1].c_str());
        SendDlgItemMessageW(dialog,IDC_COUNTDOWN_NAME1,EM_SETLIMITTEXT,8,0);SendDlgItemMessageW(dialog,IDC_COUNTDOWN_NAME2,EM_SETLIMITTEXT,8,0);
        SendDlgItemMessageW(dialog,IDC_COUNTDOWN_DATE1,EM_SETLIMITTEXT,10,0);SendDlgItemMessageW(dialog,IDC_COUNTDOWN_DATE2,EM_SETLIMITTEXT,10,0);
        return TRUE;
    }
    if(message==WM_COMMAND) {
        if(LOWORD(wParam)==IDCANCEL) {EndDialog(dialog,IDCANCEL);return TRUE;}
        if(LOWORD(wParam)==IDOK) {
            wchar_t n[2][32]{},d[2][32]{};
            GetDlgItemTextW(dialog,IDC_COUNTDOWN_NAME1,n[0],32);GetDlgItemTextW(dialog,IDC_COUNTDOWN_NAME2,n[1],32);
            GetDlgItemTextW(dialog,IDC_COUNTDOWN_DATE1,d[0],32);GetDlgItemTextW(dialog,IDC_COUNTDOWN_DATE2,d[1],32);
            for(int i=0;i<2;i++) {
                SYSTEMTIME target{};
                if(*d[i] && !Countdown::ParseDate(d[i],target)) {
                    MessageBoxW(dialog,L"Please enter a valid date in YYYY-MM-DD format, e.g. 2026-12-31.\nLeave empty to unset.",L"Invalid Date",MB_OK|MB_ICONWARNING);
                    SetFocus(GetDlgItem(dialog,i==0?IDC_COUNTDOWN_DATE1:IDC_COUNTDOWN_DATE2));return TRUE;
                }
            }
            auto path=ConfigPath();
            // Preserve Unicode names even when the configuration is created for the first time.
            if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES) {
                HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
                if(file!=INVALID_HANDLE_VALUE) {WORD bom=0xfeff;DWORD written;WriteFile(file,&bom,2,&written,nullptr);CloseHandle(file);}
            }
            bool ok=WritePrivateProfileStringW(L"countdown",L"Name1",n[0],path.c_str())!=0;
            ok=(WritePrivateProfileStringW(L"countdown",L"Name2",n[1],path.c_str())!=0)&&ok;
            ok=(WritePrivateProfileStringW(L"countdown",L"Date1",d[0],path.c_str())!=0)&&ok;
            ok=(WritePrivateProfileStringW(L"countdown",L"Date2",d[1],path.c_str())!=0)&&ok;
            if(!ok) {MessageBoxW(dialog,L"Could not save settings. Check write permissions for the data directory.",L"Save Failed",MB_OK|MB_ICONERROR);return TRUE;}
            EndDialog(dialog,IDOK);return TRUE;
        }
    }
    return FALSE;
}
class CountdownItem:public IPluginItem {
public:
    // Double-buffered: the host reads these strings on the UI thread while DataRequired may run elsewhere.
    std::wstring values[2]={L"-- / --",L"-- / --"},samples[2]={L"-9999d / -9999d",L"-9999d / -9999d"};
    volatile LONG current=0;
    void Publish(std::wstring value,std::wstring sample) {
        LONG next=1-current;values[next]=std::move(value);samples[next]=std::move(sample);
        InterlockedExchange(&current,next);
    }
    const wchar_t* GetItemName()const override {return L"Dual Countdown";}
    const wchar_t* GetItemId()const override {return L"CustomDualCountdown";}
    const wchar_t* GetItemLableText()const override {return L"";}
    const wchar_t* GetItemValueText()const override {return values[current].c_str();}
    const wchar_t* GetItemValueSampleText()const override {return samples[current].c_str();}
};
class CountdownPlugin:public ITMPlugin {
    CountdownItem item;
    std::wstring tooltips[2];
    volatile LONG tooltipCurrent=0;
public:
    CountdownPlugin() {DataRequired();}
    IPluginItem* GetItem(int i)override {return i==0?&item:nullptr;}
    void DataRequired()override {
        Load();SYSTEMTIME today{};GetLocalTime(&today);
        std::wstring value,sample,tooltip=L"Dual Countdown (local calendar days)\n";
        for(int i=0;i<2;i++) {
            auto prefix=names[i].empty()?L"":names[i]+L" ";
            if(i) {value+=L" / ";sample+=L" / ";}
            auto days=Countdown::DayText(dates[i],today);
            value+=prefix+days;sample+=prefix+L"-9999d";
            tooltip+=prefix+(dates[i].empty()?L"Not set":dates[i])+L": "+days+L"\n";
        }
        tooltip+=L"0d means today; negative means past. Use Options to edit target dates.";
        item.Publish(std::move(value),std::move(sample));
        LONG next=1-tooltipCurrent;tooltips[next]=std::move(tooltip);InterlockedExchange(&tooltipCurrent,next);
    }
    const wchar_t* GetTooltipInfo()override {return tooltips[tooltipCurrent].c_str();}
    const wchar_t* GetInfo(PluginInfoIndex i)override {
        switch(i) {case TMI_NAME:return L"Dual Countdown";case TMI_DESCRIPTION:return L"Displays remaining days for two custom target dates";
        case TMI_AUTHOR:return L"Local Countdown Plugin";case TMI_VERSION:return L"1.0.0";default:return L"";}
    }
    OptionReturn ShowOptionsDialog(void* parent)override {
        Load();auto result=DialogBoxParamW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(IDD_BUILTIN_COUNTDOWN),static_cast<HWND>(parent),SettingsProc,0);
        if(result==IDOK) {DataRequired();return OR_OPTION_CHANGED;}
        return OR_OPTION_UNCHANGED;
    }
};
}
ITMPlugin* Builtin::DualCountdown() { static CountdownPlugin feature; return &feature; }
