#include "Profiler.hpp"
#include "Engine/Timer.hpp"
#include "Common/Hash.hpp"
#include "Graphics/TextRenderManager.hpp"

#include <iomanip>
#include <unordered_map>
#include <glm/glm.hpp>
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

        CommandBuffer *command_buffer;

        bool is_cpu_profiler() {
            return command_buffer == nullptr;
        }
    };

    uint32_t query_indices[2] = {0, 0};
    uint32_t frame_id = 1;

    QueryID gpu_query_pools[2];

    bool enabled = true;
    std::unordered_map<uint32_t, Range> ranges;

    void Initialize() {
        if (!enabled)
            return;
        RenderingDevice *device = RenderingDevice::get();
        gpu_query_pools[0] = device->create_query(K_MAX_QUERY_COUNT);
        gpu_query_pools[1] = device->create_query(K_MAX_QUERY_COUNT);
        std::memset(query_results, 0, sizeof(uint64_t) * K_MAX_QUERY_COUNT);
        gpu_timestamp_period = device->get_timestamp_period();
    }

    void NewFrame(CommandBuffer *command_buffer) {
        if (!enabled)
            return;

        frame_id = 1 - frame_id;
        RenderingDevice::get()->reset_query(command_buffer, gpu_query_pools[frame_id], 0, K_MAX_QUERY_COUNT);

        query_indices[frame_id] = 0;
    }

    uint32_t BeginRangeCPU(const char *name) {
        if (!enabled)
            return 0;

        uint32_t id = utils::djb2_hash_string(name);
        ranges[id].name = name;
        ranges[id].avg_counter += 1;
        ranges[id].cpu_timer.record();
        ranges[id].command_buffer = nullptr;
        return id;
    }

    uint32_t BeginRangeGPU(CommandBuffer *command_buffer, const char *name) {
        if (!enabled)
            return 0;

        uint32_t id = utils::djb2_hash_string(name);
        ranges[id].name = name;
        ranges[id].avg_counter += 1;
        ranges[id].command_buffer = command_buffer;

        uint32_t current_query_index = query_indices[frame_id]++;
        RenderingDevice::get()->query(command_buffer, gpu_query_pools[frame_id], current_query_index);

        ranges[id].query_index_begin = current_query_index;
        return id;
    }

    void EndRange(uint32_t id) {
        if (!enabled)
            return;

        if (ranges[id].is_cpu_profiler()) {
            ranges[id].time += (float)ranges[id].cpu_timer.elapsed_milliseconds();
        } else {
            uint32_t current_query_index = query_indices[frame_id]++;
            RenderingDevice::get()->query(ranges[id].command_buffer, gpu_query_pools[frame_id], current_query_index);
            ranges[id].query_index_end = current_query_index;
        }
    }

    void DrawData() {
        if (!enabled)
            return;

        uint32_t prev_frame = 1 - frame_id;
        uint32_t query_count = query_indices[prev_frame];

        if (query_count > 0)
            RenderingDevice::get()->resolve_query(gpu_query_pools[prev_frame], query_results, 0, query_count);

        TextRenderer *renderer = TextRenderManager::get()->get_default();
        std::stringstream ss("");
        glm::vec2 position = {5.0f, 34.0f};
        float font_size = 12.0f;
        for (auto &[key, val] : ranges) {
            // Skip for first frame
            if (val.avg_counter == 1)
                continue;

            float avg_time = 0.0f;
            if (val.is_cpu_profiler()) {
                avg_time = val.time / val.avg_counter;
            } else {
                // GPU is always one frame behind
                float delta = (float(query_results[val.query_index_end] - query_results[val.query_index_begin]) * gpu_timestamp_period) / 1000000.0f;
                val.time += delta;
                avg_time = delta;
            }
            ss << val.name << "(" << std::fixed << std::setprecision(2) << avg_time << "ms)";
            renderer->AddText(ss.str(), position, font_size);
            ss.str("");
            position.y += font_size + 2.0f;
        }
        std::memset(query_results, 0, sizeof(uint64_t) * K_MAX_QUERY_COUNT);
    }

    void Destroy() {
        if (!enabled)
            return;
        RenderingDevice::get()->destroy_queries(gpu_query_pools, 2);
    }
} // namespace mirai::miProfiler