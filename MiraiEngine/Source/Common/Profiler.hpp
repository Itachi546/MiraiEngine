#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#define MI_PROFILER_CONCAT(x, y) x##y
#define MI_PROFILER_CONCAT_INNER(x, y) MI_PROFILER_CONCAT(x, y)
#define ScopedCpuProfiling(name) mirai::miProfiler::ScopeRangeCPU MI_PROFILER_CONCAT_INNER(_wi_profiler_cpu_range, __LINE__)(name)
#define ScopedGpuProfiling(command_buffer, name) mirai::miProfiler::ScopeRangeGPU MI_PROFILER_CONCAT_INNER(_wi_profiler_gpu_range, __LINE__)(command_buffer, name)

namespace mirai {
    class CommandBuffer;
}

namespace mirai::miProfiler {

    struct ProfilerOutput {
        std::string name;
        float time_in_ms;
        uint32_t sort_index;
    };

    void Initialize();

    void BeginFrame(CommandBuffer *command_buffer);

    bool IsEnabled();

    void GetProfilerOutput(std::vector<ProfilerOutput> &cpu_profiler_output, std::vector<ProfilerOutput> &gpu_profiler_output);

    void Destroy();

    uint32_t BeginRangeGPU(CommandBuffer *command_buffer, const char *name);

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

    struct ScopeRangeGPU {
        uint32_t id;
        inline ScopeRangeGPU(CommandBuffer *command_buffer, const char *name) {
            id = BeginRangeGPU(command_buffer, name);
        }
        inline ~ScopeRangeGPU() {
            EndRange(id);
        }
    };

    void DrawData();
} // namespace mirai::miProfiler
