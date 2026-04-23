#pragma once

#include <stdint.h>
#include <functional>
#include <atomic>

// https://turanszkij.wpcomstaging.com/2018/11/simple-job-system-using-standard-c/
namespace mirai::jobsystem {
    struct JobDispatchArg {
        uint32_t job_index;
        uint32_t group_index;
    };

    void Initialize(uint32_t max_threads = ~0u);

    void Execute(const std::function<void()> &job);
    void Dispatch(uint32_t job_count, uint32_t group_size, const std::function<void(JobDispatchArg)> &job);

    bool IsBusy();

    void Wait();

    void Shutdown();
} // namespace mirai::jobsystem