#include "Profiler.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Engine/Timer.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include "Graphics/TextRenderManager.hpp"
#include "Math/Math.hpp"
#include "Engine/AppSettings.hpp"

#include <iomanip>
#include <sstream>
#include <cstring>

namespace mirai::miProfiler {
    const uint32_t K_MAX_QUERY_COUNT = 64;
    uint64_t query_results[K_MAX_QUERY_COUNT];
    float gpu_timestamp_period = 0.0f;

    struct Range {
        std::string name;
        int avg_counter = 0;
        float time = 0.0f;

        Timer cpu_timer;

        int query_index_begin = -1;
        int query_index_end = -1;
        bool updated_last_frame = false;
        uint32_t sort_index = K_INVALID_ID;

        CommandBuffer *command_buffer;

        bool is_cpu_profiler() {
            return command_buffer == nullptr;
        }
    };

    const uint32_t k_total_frames = AppSettings::K_MAX_FRAME_IN_FLIGHTS;
    uint32_t query_indices[k_total_frames] = {};
    uint32_t frame_id = 1;

    QueryID gpu_query_pools[k_total_frames];

    bool enabled = true;
    uint32_t range_sort_id = 0;
    HashMap<uint32_t, Range> ranges;

    void Initialize() {
        if (!enabled)
            return;

        RenderingDevice *device = RenderingDevice::get();
        for (int i = 0; i < k_total_frames; ++i) {
            gpu_query_pools[i] = device->create_query(K_MAX_QUERY_COUNT);
            query_indices[i] = 0;
        }

        std::memset(query_results, 0, sizeof(uint64_t) * K_MAX_QUERY_COUNT);
        gpu_timestamp_period = device->get_timestamp_period();
    }

    void BeginFrame(CommandBuffer *command_buffer) {
        if (!enabled)
            return;

        frame_id = (frame_id + 1) % k_total_frames;

        // Get query result
        std::memset(query_results, 0, sizeof(uint64_t) * K_MAX_QUERY_COUNT);
        uint32_t query_count = query_indices[frame_id];
        if (query_count > 0) {
            RenderingDevice::get()->resolve_query(gpu_query_pools[frame_id], query_results, 0, query_count);
        }

        RenderingDevice::get()->reset_query(command_buffer, gpu_query_pools[frame_id], 0, K_MAX_QUERY_COUNT);

        query_indices[frame_id] = 0;
    }

    bool IsEnabled() {
        return enabled;
    }

    uint32_t BeginRangeCPU(const char *name) {
        if (!enabled)
            return 0;

        uint32_t id = utils::djb2_hash_string(name);
        auto found = ranges.find(id);
        if (found == ranges.end()) {
            ranges.insert(std::make_pair(id, Range{.sort_index = range_sort_id++}));
            found = ranges.find(id);
        }
        found->second.name = name;
        found->second.avg_counter += 1;
        found->second.cpu_timer.record();
        found->second.command_buffer = nullptr;
        found->second.updated_last_frame = true;
        return id;
    }

    uint32_t BeginRangeGPU(CommandBuffer *command_buffer, const char *name) {
        if (!enabled)
            return 0;

        uint32_t id = utils::djb2_hash_string(name);
        auto found = ranges.find(id);
        if (found == ranges.end()) {
            ranges.insert(std::make_pair(id, Range{.sort_index = range_sort_id++}));
            found = ranges.find(id);
        }

        found->second.name = name;
        found->second.avg_counter += 1;
        found->second.command_buffer = command_buffer;
        found->second.updated_last_frame = true;

        uint32_t current_query_index = query_indices[frame_id]++;
        RenderingDevice::get()->query(command_buffer, gpu_query_pools[frame_id], current_query_index);

        ranges[id].query_index_begin = current_query_index;
        return id;
    }

    void EndRange(uint32_t id) {
        if (!enabled)
            return;

        if (ranges[id].is_cpu_profiler()) {
            ranges[id].time = (float)ranges[id].cpu_timer.elapsed_milliseconds();
        } else {
            uint32_t current_query_index = query_indices[frame_id]++;
            RenderingDevice::get()->query(ranges[id].command_buffer, gpu_query_pools[frame_id], current_query_index);
            ranges[id].query_index_end = current_query_index;
        }
    }

    void GetProfilerOutput(std::vector<ProfilerOutput> &cpu_profiler_output, std::vector<ProfilerOutput> &gpu_profiler_output) {
        if (!enabled)
            return;
        for (auto &[key, val] : ranges) {
            // Skip for first frame
            if (val.avg_counter == 1 || !val.updated_last_frame)
                continue;

            if (val.is_cpu_profiler()) {
                cpu_profiler_output.emplace_back(val.name, val.time, val.sort_index);
            } else {
                // GPU is always one frame behind
                float delta = (float(query_results[val.query_index_end] - query_results[val.query_index_begin]) * gpu_timestamp_period) / 1000000.0f;
                val.time = delta;
                gpu_profiler_output.emplace_back(val.name, delta, val.sort_index);
            }
            val.updated_last_frame = false;
        }

        std::sort(cpu_profiler_output.begin(), cpu_profiler_output.end(), [](const ProfilerOutput &a, const ProfilerOutput &b) {
            return a.sort_index < b.sort_index;
        });
        std::sort(gpu_profiler_output.begin(), gpu_profiler_output.end(), [](const ProfilerOutput &a, const ProfilerOutput &b) {
            return a.sort_index < b.sort_index;
        });
    }

    void DrawData() {
        if (!enabled)
            return;
        /*
        TextRenderer *renderer = TextRenderManager::get()->get_default();
        std::stringstream ss("");
        glm::vec2 position = {5.0f, 34.0f};
        float font_size = 14.0f;

        std::vector<ProfilerOutput> cpu, gpu;
        GetProfilerOutput(cpu, gpu);

        for (auto &[name, time] : cpu) {
            ss << name << "(" << std::fixed << std::setprecision(2) << time << "ms)";
            renderer->AddText(ss.str(), position, font_size);
            ss.str("");
            position.y += font_size + 2.0f;
        }

        for (auto &[name, time] : gpu) {
            ss << name << "(" << std::fixed << std::setprecision(2) << time << "ms)";
            renderer->AddText(ss.str(), position, font_size);
            ss.str("");
            position.y += font_size + 2.0f;
        }
        */
    }

    void Destroy() {
        if (!enabled)
            return;
        RenderingDevice::get()->destroy_queries(gpu_query_pools, 2);
    }
} // namespace mirai::miProfiler