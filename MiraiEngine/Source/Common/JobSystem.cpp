#include "JobSystem.hpp"
#include "ThreadSafeRingBuffer.hpp"

#include <thread>
#include <vector>
#include <condition_variable>

namespace mirai::jobsystem {

    struct JobState {
        uint32_t num_threads;
        std::vector<std::thread> threads;
        ThreadSafeRingBuffer<std::function<void()>, 256> job_pool;
        std::condition_variable wake_condition;
        std::mutex wake_mutex;

        uint32_t current_label;
        // Track the state of finished label across the backgroud thread
        std::atomic<uint64_t> finished_label;
        std::atomic<bool> stop_flag{false};
    } g_state;

    void Initialize(uint32_t max_threads) {
        // Reset all state so repeated Initialize/Shutdown cycles work correctly.
        g_state.stop_flag.store(false);
        g_state.threads.clear();
        g_state.finished_label.store(0);
        g_state.current_label = 0;

        uint32_t num_threads = std::thread::hardware_concurrency() - 1;
        g_state.num_threads = std::max(std::min(max_threads, num_threads), 1u);

        for (uint32_t i = 0; i < g_state.num_threads; ++i) {
            g_state.threads.emplace_back(
                []() {
                    std::function<void()> job;
                    while (!g_state.stop_flag.load()) {
                        if (g_state.job_pool.pop_front(job)) {
                            job();
                            g_state.finished_label.fetch_add(1);
                        } else {
                            std::unique_lock<std::mutex> lk(g_state.wake_mutex);
                            // Wait until stop is requested or a job is available.
                            // The predicate is checked atomically under the lock,
                            // preventing missed wakeups from Shutdown()/Execute().
                            g_state.wake_condition.wait(lk, [] {
                                return g_state.stop_flag.load() ||
                                       !g_state.job_pool.empty();
                            });
                        }
                    }
                });
        }
    }

    inline void poll() {
        g_state.wake_condition.notify_one();
        std::this_thread::yield();
    }

    void Execute(const std::function<void()> &job) {
        g_state.current_label += 1;

        while (!g_state.job_pool.push_back(job)) {
            poll();
        }

        g_state.wake_condition.notify_one();
    }

    bool IsBusy() {
        return g_state.finished_label.load() < g_state.current_label;
    }

    void Wait() {
        while (IsBusy()) {
            poll();
        }
    }

    void Dispatch(uint32_t job_count, uint32_t group_size, const std::function<void(JobDispatchArg)> &job) {
        if (job_count == 0 || group_size == 0)
            return;

        const uint32_t group_count = (job_count + group_size - 1) / group_size;
        g_state.current_label += group_count;

        for (uint32_t group_index = 0; group_index < group_count; ++group_index) {
            auto job_group = [job_count, group_size, job, group_index]() {
                const uint32_t group_job_offset = group_index * group_size;
                const uint32_t group_job_end = std::min(group_job_offset + group_size, job_count);

                JobDispatchArg args;
                args.group_index = group_index;

                for (uint32_t i = group_job_offset; i < group_job_end; ++i) {
                    args.job_index = i;
                    job(args);
                }
            };

            while (!g_state.job_pool.push_back(job_group)) {
                poll();
            }
            g_state.wake_condition.notify_one();
        }
    }

    void Shutdown() {
        Wait(); // drain all pending jobs first
        g_state.stop_flag.store(true);
        g_state.wake_condition.notify_all(); // wake sleeping threads so they can exit
        for (auto &thread : g_state.threads)
            thread.join();
        g_state.threads.clear(); // clear so next Initialize() starts fresh
    }

} // namespace mirai::jobsystem