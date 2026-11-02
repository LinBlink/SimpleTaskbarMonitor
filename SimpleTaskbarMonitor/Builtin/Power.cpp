#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <winioctl.h>
#include <batclass.h>
#include <mutex>
#include <vector>
#include "BuiltinFeatures.h"
#include "PowerValues.h"
#pragma comment(lib,"setupapi.lib")
namespace {
const GUID BatteryClass={0x72631e54,0x78a4,0x11d0,{0xbc,0xf7,0x00,0xaa,0x00,0xb7,0xb3,0x2a}};
std::mutex snapshotMutex;
Builtin::PowerSnapshot snapshot;
Builtin::PowerSnapshot QueryPower() {
    Builtin::PowerSnapshot result;
    SYSTEM_POWER_STATUS system{};
    if(GetSystemPowerStatus(&system)) {
        result.present=system.BatteryFlag!=255 && !(system.BatteryFlag&128);
        result.ac=system.ACLineStatus==255?-1:system.ACLineStatus;
        result.percent=system.BatteryLifePercent<=100?system.BatteryLifePercent:-1;
        result.charging=system.BatteryFlag!=255 && (system.BatteryFlag&8)!=0;
        if(result.present && system.BatteryLifeTime!=DWORD(-1)) result.hours=system.BatteryLifeTime/3600.0;
    }
    auto devices=SetupDiGetClassDevsW(&BatteryClass,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
    if(devices==INVALID_HANDLE_VALUE) return result;
    double capacity=0, rate=0;
    bool knownCapacity=true, knownRate=true;
    int batteries=0;
    for(DWORD i=0;i<64;++i) {
        SP_DEVICE_INTERFACE_DATA iface{}; iface.cbSize=sizeof(iface);
        if(!SetupDiEnumDeviceInterfaces(devices,nullptr,&BatteryClass,i,&iface)) break;
        DWORD bytes=0;
        SetupDiGetDeviceInterfaceDetailW(devices,&iface,nullptr,0,&bytes,nullptr);
        if(bytes<sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W)) continue;
        std::vector<BYTE> buffer(bytes);
        auto detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(buffer.data());
        detail->cbSize=sizeof(*detail);
        if(!SetupDiGetDeviceInterfaceDetailW(devices,&iface,detail,bytes,nullptr,nullptr)) continue;
        HANDLE battery=CreateFileW(detail->DevicePath,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(battery==INVALID_HANDLE_VALUE) continue;
        DWORD wait=0,returned=0;
        BATTERY_QUERY_INFORMATION query{};
        BATTERY_INFORMATION info{};
        BATTERY_WAIT_STATUS request{};
        BATTERY_STATUS status{};
        bool ok=DeviceIoControl(battery,IOCTL_BATTERY_QUERY_TAG,&wait,sizeof(wait),&query.BatteryTag,sizeof(query.BatteryTag),&returned,nullptr)!=FALSE && query.BatteryTag!=0;
        query.InformationLevel=BatteryInformation;
        ok=ok && DeviceIoControl(battery,IOCTL_BATTERY_QUERY_INFORMATION,&query,sizeof(query),&info,sizeof(info),&returned,nullptr)!=FALSE;
        request.BatteryTag=query.BatteryTag;
        ok=ok && DeviceIoControl(battery,IOCTL_BATTERY_QUERY_STATUS,&request,sizeof(request),&status,sizeof(status),&returned,nullptr)!=FALSE;
        CloseHandle(battery);
        if(!ok || !(info.Capabilities&BATTERY_SYSTEM_BATTERY) || (info.Capabilities&BATTERY_IS_SHORT_TERM)) continue;
        ++batteries; result.present=true;
        bool absolute=(info.Capabilities&BATTERY_CAPACITY_RELATIVE)==0;
        knownCapacity=knownCapacity && absolute && status.Capacity!=BATTERY_UNKNOWN_CAPACITY;
        knownRate=knownRate && absolute && status.Rate!=BATTERY_UNKNOWN_RATE;
        if(absolute && status.Capacity!=BATTERY_UNKNOWN_CAPACITY) capacity+=status.Capacity;
        if(absolute && status.Rate!=BATTERY_UNKNOWN_RATE) rate+=status.Rate;
        // Voltage is that of the first reporting system battery; never sum voltages.
        if(!std::isfinite(result.volts) && status.Voltage!=BATTERY_UNKNOWN_VOLTAGE) result.volts=status.Voltage/1000.0;
    }
    SetupDiDestroyDeviceInfoList(devices);
    if(batteries) {
        if(knownCapacity) result.wattHours=capacity/1000.0;
        if(knownRate) result.watts=rate/1000.0;
        if(!std::isfinite(result.hours) && knownCapacity && knownRate && rate<0) result.hours=capacity/-rate;
    }
    return result;
}
class ValueItem: public IPluginItem {
public:
    const wchar_t* name; const wchar_t* id; const wchar_t* label; const wchar_t* sample;
    std::wstring value=L"--";
    ValueItem(const wchar_t* n,const wchar_t* i,const wchar_t* l,const wchar_t* s):name(n),id(i),label(l),sample(s){}
    const wchar_t* GetItemName()const override{return name;}
    const wchar_t* GetItemId()const override{return id;}
    const wchar_t* GetItemLableText()const override{return label;}
    const wchar_t* GetItemValueText()const override{return value.c_str();}
    const wchar_t* GetItemValueSampleText()const override{return sample;}
};
std::wstring PowerTooltip(const Builtin::PowerSnapshot& s) {
    if(!s.present) return L"未检测到系统电池；不支持的读数显示 --。";
    auto supply=s.ac==1?L"外接电源":s.ac==0?L"电池供电":L"电源状态未知";
    return std::wstring(supply)+(s.charging?L"，充电中":L"")+L"\n电量："+Builtin::Percentage(s)+
        L"\n电池充放电功率："+Builtin::Measurement(s.watts,L" W")+
        L"\n剩余容量："+Builtin::Measurement(s.wattHours,L" Wh")+
        L"\n电压（首块电池）："+Builtin::Measurement(s.volts,L" V")+
        L"\n估计续航："+Builtin::Measurement(s.hours,L" h")+
        L"\n正功率表示充电，负功率表示放电；该读数不是整机插座功耗。";
}
class BatteryFeature: public ITMPlugin {
    ValueItem item{L"电池电量",L"b5R30ITQ",L"电池: ",L"100%"};
    std::wstring tooltip;
public:
    IPluginItem* GetItem(int i)override{return i==0?&item:nullptr;}
    void DataRequired()override{
        auto state=Builtin::GetPowerSnapshot();
        item.value=Builtin::Percentage(state);
        const auto config=Builtin::DataDirectory()+L"\\Battery.dll.ini";
        if(!GetPrivateProfileIntW(L"config",L"show_percent",1,config.c_str()) && item.value!=L"--")
            item.value.pop_back();
        tooltip=PowerTooltip(state);
    }
    const wchar_t* GetTooltipInfo()override{return tooltip.c_str();}
    const wchar_t* GetInfo(PluginInfoIndex i)override{
        switch(i){case TMI_NAME:return L"电池";case TMI_DESCRIPTION:return L"内置 Windows 电池电量与充电状态";case TMI_AUTHOR:return L"SimpleTaskbarMonitor";case TMI_VERSION:return L"1.0";default:return L"";}
    }
};
class PowerFeature: public ITMPlugin {
    ValueItem items[5]={{L"电池充放电功率",L"BatteryPowerMon",L"PWR: ",L"-100.0W"},
        {L"电池电量百分比",L"BatteryPercentageMon",L"Battery ",L"100%"},
        {L"电池剩余容量",L"BatteryCapacityMon",L"Cap: ",L"100.0Wh"},
        {L"电池电压",L"BatteryVoltageMon",L"Volt: ",L"20.0V"},
        {L"电池预计续航",L"BatteryTimeMon",L"Time: ",L"100.0h"}};
    std::wstring tooltip;
public:
    IPluginItem* GetItem(int i)override{return i>=0 && i<5?&items[i]:nullptr;}
    void DataRequired()override{
        auto state=Builtin::GetPowerSnapshot();
        const auto config=Builtin::DataDirectory()+L"\\PowerMonPlugin.dll.ini";
        wchar_t powerUnit[64]{}, capacityUnit[64]{}, voltageUnit[64]{};
        GetPrivateProfileStringW(L"config",L"pwr_unit_str",L"W",powerUnit,64,config.c_str());
        GetPrivateProfileStringW(L"config",L"electric_capacity_unit_str",L"Wh",capacityUnit,64,config.c_str());
        GetPrivateProfileStringW(L"config",L"electric_voltage_unit_str",L"V",voltageUnit,64,config.c_str());
        items[0].value=Builtin::Measurement(state.watts,powerUnit);
        items[1].value=Builtin::Percentage(state);
        items[2].value=Builtin::Measurement(state.wattHours,capacityUnit);
        items[3].value=Builtin::Measurement(state.volts,voltageUnit);
        items[4].value=Builtin::Measurement(state.hours,L"h");
        tooltip=PowerTooltip(state);
    }
    const wchar_t* GetTooltipInfo()override{return tooltip.c_str();}
    const wchar_t* GetInfo(PluginInfoIndex i)override{
        switch(i){case TMI_NAME:return L"功耗";case TMI_DESCRIPTION:return L"内置电池功率、容量、电压、续航，无需额外驱动";case TMI_AUTHOR:return L"SimpleTaskbarMonitor";case TMI_VERSION:return L"1.0";default:return L"";}
    }
};
}
Builtin::PowerSnapshot Builtin::GetPowerSnapshot(){std::lock_guard<std::mutex> lock(snapshotMutex);return snapshot;}
void Builtin::SampleHardware(){auto next=QueryPower();std::lock_guard<std::mutex> lock(snapshotMutex);snapshot=next;}
ITMPlugin* Builtin::Battery(){static BatteryFeature feature;return &feature;}
ITMPlugin* Builtin::Power(){static PowerFeature feature;return &feature;}
