#include "Profiler.hpp"
#include "Engine/Timer.hpp"
#include "Common/Hash.hpp"
#include "Graphics/TextRenderManager.hpp"

#include <iomanip>
#include <unordered_map>
#include <glm/glm.hpp>
#include <sstream>

namespace mirai {
namespace miProfiler {

    struct Range {
        std::string name;
        int avg_counter = 0;
        float time = 0.0f;

        Timer cpu_timer;
        bool is_cpu_timer = true;
    };

    bool enabled = true;
    std::unordered_map<uint32_t, Range> ranges;

    uint32_t BeginRangeCPU(const char *name) {
        if (!enabled)
            return 0;

        uint32_t id = utils::djb2_hash_string(name);
        ranges[id].name = name;
        ranges[id].avg_counter += 1;
        ranges[id].cpu_timer.record();
        ranges[id].is_cpu_timer = true;
        return id;
    }

    void EndRange(uint32_t id) {
        if (!enabled)
            return;

        if (ranges[id].is_cpu_timer) {
            ranges[id].time += (float)ranges[id].cpu_timer.elapsed_milliseconds();
        }
    }

    void DrawData() {
        if (!enabled)
            return;

        TextRenderer *renderer = TextRenderManager::get()->get_default();

        std::stringstream ss("");
        glm::vec2 position = {5.0f, 20.0f};
        float font_size = 12.0f;
        for (auto &[key, val] : ranges) {
            // Skip for first frame
            if (val.avg_counter == 1)
                continue;

            float avg_time = val.time / val.avg_counter;
            ss << val.name << "(" << std::fixed << std::setprecision(2) << avg_time << "ms)";
            renderer->AddText(ss.str(), position, font_size);
            ss.str("");
            position.y += font_size + 2.0f;
        }
    }
}
} // namespace mirai::miProfiler