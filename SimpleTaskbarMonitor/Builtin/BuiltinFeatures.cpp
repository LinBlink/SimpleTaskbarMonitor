#include <windows.h>
#include "BuiltinFeatures.h"
namespace Builtin {
std::wstring DataDirectory() {
    wchar_t path[32768]{};
    GetModuleFileNameW(nullptr,path,32768);
    std::wstring directory(path);
    directory.resize(directory.find_last_of(L"\\/"));
    directory += L"\\plugins";
    CreateDirectoryW(directory.c_str(),nullptr);
    return directory;
}
std::vector<ITMPlugin*> Features() { return {AIUsage(),DualCountdown(),Battery(),Power()}; }
}
