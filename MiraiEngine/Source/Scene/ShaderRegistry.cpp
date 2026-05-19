#include "ShaderRegistry.hpp"
#include "Common/Hash.hpp"
#include "Shader.hpp"

namespace mirai {
    ShaderRegistry *ShaderRegistry::Instance = nullptr;

    ShaderRegistry::ShaderRegistry() {
        ASSERT(Instance == nullptr);
        Instance = this;
    }

    void ShaderRegistry::destroy() {
        std::vector<PipelineID> pipelines;
        pipelines.reserve(table.size());

        for (auto &[key, val] : table) {
            pipelines.push_back(val->pipeline_id);
        }
        RenderingDevice::get()->destroy_pipelines(pipelines.data(), cast_u32(pipelines.size()));
        table.clear();
    }
} // namespace mirai