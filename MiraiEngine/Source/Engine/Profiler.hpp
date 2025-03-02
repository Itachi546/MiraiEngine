#pragma once
#include <stdint.h>
#include <string>
#include <vector>

#define ScopedCpuProfiling(name) mirai::miProfiler::ScopeRangeCPU MI_PROFILER_CONCAT(_wi_profiler_cpu_range, __LINE__)(name)
#define ScopedGpuProfiling(command_buffer, name) mirai::miProfiler::ScopeRangeGPU MI_PROFILER_CONCAT(_wi_profiler_gpu_range, __LINE__)(command_buffer, name)
#define MI_PROFILER_CONCAT(x, y) x##y

namespace mirai {
    class CommandBuffer;
}

namespace mirai::miProfiler {

    struct ProfilerOutput {
        std::string name;
        float time_in_ms;
    };

    void Initialize();

    void BeginFrame(CommandBuffer *command_buffer);

    bool IsEnabled();

    void EndFrame();

    void GetProfilerOutput(std::vector<ProfilerOutput>& cpu_profiler_output, std::vector<ProfilerOutput> &gpu_profiler_output);

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
