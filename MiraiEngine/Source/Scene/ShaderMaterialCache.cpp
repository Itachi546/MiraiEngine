#include "ShaderMaterialCache.hpp"

namespace mirai {
    ShaderMaterialCache *ShaderMaterialCache::Instance = nullptr;

    ShaderMaterialCache::ShaderMaterialCache() {
        Instance = this;
    }

    void ShaderMaterialCache::cache_shader(uint32_t hash, ShaderID shader) {
        if (shader_caches.find(hash) == shader_caches.end())
            shader_caches[hash] = shader;
    }

    ShaderID ShaderMaterialCache::get_shader(uint32_t hash) {
        auto found = shader_caches.find(hash);
        if (found != shader_caches.end())
            return found->second;
        return ShaderID{K_INVALID_ID};
    }

    PipelineID ShaderMaterialCache::get_pipeline(uint64_t hash) {
        auto found = pipeline_caches.find(hash);
        if (found != pipeline_caches.end())
            return found->second;
        return PipelineID{K_INVALID_ID};
    }

    void ShaderMaterialCache::add_pipeline(uint64_t hash, PipelineID pipeline) {
        pipeline_caches[hash] = pipeline;
    }

    ShaderMaterialCache::~ShaderMaterialCache() {
        for (auto &[key, val] : shader_caches) {
            RenderingDevice::get()->destroy_shaders(&val, 1);
        }

        for (auto &[key, val] : pipeline_caches) {
            RenderingDevice::get()->destroy_pipelines(&val, 1);
        }
    }
} // namespace mirai