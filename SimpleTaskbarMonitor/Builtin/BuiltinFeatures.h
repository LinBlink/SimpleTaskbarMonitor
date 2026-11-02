#pragma once
#include "PluginInterface.h"
#include <string>
#include <vector>
namespace Builtin {
std::vector<ITMPlugin*> Features();
std::wstring DataDirectory();
void SampleHardware(); // Worker thread: publishes a value snapshot, never UI objects.
ITMPlugin* AIUsage();
void RequestAIUsageRefresh();
bool IsAIResetHighlighted(const IPluginItem* item, unsigned long long tick);
ITMPlugin* DualCountdown();
ITMPlugin* Battery();
ITMPlugin* Power();
}
