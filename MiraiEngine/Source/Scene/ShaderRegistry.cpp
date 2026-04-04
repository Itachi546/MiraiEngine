#include "ShaderRegistry.hpp"
#include "Common/Hash.hpp"
#include "Shader.hpp"

namespace mirai {
    ShaderRegistryMap *ShaderRegistryMap::Instance = nullptr;

    ShaderRegistryMap::ShaderRegistryMap() {
        ASSERT(Instance == nullptr);
        Instance = this;
    }

    ShaderRegistry *ShaderRegistryMap::add_registry(PassMode pass_mode, std::shared_ptr<ShaderRegistry> shader) {
        auto found = shader_registry_map.find(pass_mode);
        if (found != shader_registry_map.end()) {
            ASSERT_MSG(0, "Material variant already created");
            return shader_registry_map[pass_mode].get();
        }

        shader_registry_map.insert(std::make_pair(pass_mode, shader));
        return shader.get();
    }

    ShaderRegistry *ShaderRegistryMap::get_registry(PassMode pass_mode) {
        auto found = shader_registry_map.find(pass_mode);
        if (found != shader_registry_map.end())
            return found->second.get();
        return nullptr;
    }

    void ShaderRegistryMap::destroy() {
        std::vector<PipelineID> pipelines;
        pipelines.reserve(shader_registry_map.size());
        for (auto &[pass, registry] : shader_registry_map) {
            for (auto &[key, val] : registry->table) {
                pipelines.push_back(val->pipeline_id);
            }
        }
        RenderingDevice::get()->destroy_pipelines(pipelines.data(), cast_u32(pipelines.size()));
        shader_registry_map.clear();
    }

    /*
    Shader *ShaderRegistryMap::add_shader(uint32_t shader_id, std::shared_ptr<Shader> shader) {
        auto found = shader_map.find(shader_id);
        if (found != shader_map.end()) {
            ASSERT_MSG(0, "Shader already exists");
            return found->second.get();
        }
        shader_map.insert(std::make_pair(shader_id, shader));
        return shader.get();
    }

    Shader *ShaderRegistryMap::get_shader(uint32_t shader_id) {
        auto found = shader_map.find(shader_id);
        if (found == shader_map.end()) {
            ASSERT_MSG(0, "Shader not found");
            return nullptr;
        }
        return found->second.get();
    }
    */
} // namespace mirai