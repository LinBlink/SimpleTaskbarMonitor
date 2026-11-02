#pragma once
#include <cstdint>

namespace Builtin {
// Wall time detects quota deadlines; monotonic time controls the visible alert.
class ResetAlert {
    std::uint64_t deadline{}, handled{}, started{};
    bool active{};
public:
    static constexpr std::uint64_t Duration = 30000;
    void Observe(std::uint64_t reset, std::uint64_t epoch, std::uint64_t tick) {
        if (deadline && epoch >= deadline) {
            handled = deadline;
            deadline = 0;
            started = tick;
            active = true;
        }
        // Unknown/expired startup caches cannot arm an alert. A new future
        // deadline replaces an old prediction without claiming a reset occurred.
        if (reset > epoch && reset > handled) deadline = reset;
    }
    bool Highlight(std::uint64_t tick) const {
        return active && tick >= started && tick - started < Duration
            && ((tick - started) / 500) % 2 == 0;
    }
};
}
