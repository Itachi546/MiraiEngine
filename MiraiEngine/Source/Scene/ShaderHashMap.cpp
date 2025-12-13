#include "ShaderHashMap.hpp"
#include "Common/Hash.hpp"
#include "Shader.hpp"

namespace mirai {
    ShaderHashMap *ShaderHashMap::Instance = nullptr;

    ShaderHashMap::ShaderHashMap() {
        ASSERT(Instance == nullptr);
        Instance = this;
    }

    Shader *ShaderHashMap::add(uint64_t shader_key, std::shared_ptr<Shader> shader) {
        auto found = shader_map.find(shader_key);
        if (found != shader_map.end())
            return found->second.get();

        shader_map.insert(std::make_pair(shader_key, shader));
        return shader.get();
    }

    Shader *ShaderHashMap::get(uint64_t shader_key) {
        auto found = shader_map.find(shader_key);
        if (found != shader_map.end())
            return found->second.get();
        return nullptr;
    }

    void ShaderHashMap::destroy() {
        std::vector<PipelineID> pipelines;
        pipelines.reserve(shader_map.size());
        for (auto &[key, val] : shader_map)
            pipelines.push_back(val->pipeline_id);
        RenderingDevice::get()->destroy_pipelines(pipelines.data(), cast_u32(pipelines.size()));
        shader_map.clear();
    }

    ShaderHashMap::~ShaderHashMap() {
    }

} // namespace mirai