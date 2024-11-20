#pragma once
#include <stdint.h>

#define ScopedCpuProfiling(name) mirai::miProfiler::ScopeRangeCPU MI_PROFILER_CONCAT(_wi_profiler_cpu_range, __LINE__)(name)
#define MI_PROFILER_CONCAT(x, y) x##y

namespace mirai {
namespace miProfiler {
    uint32_t BeginRangeCPU(const char *name);

    void EndRange(uint32_t id);

    struct ScopeRangeCPU {
        uint32_t id;

        inline ScopeRangeCPU(const char *name) {
            id = BeginRangeCPU(name);
        }

        inline ~ScopeRangeCPU() {
            EndRange(id);
        }
    };

    void DrawData();
}

} // namespace mirai::miProfiler