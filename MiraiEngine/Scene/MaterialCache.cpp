#include "MaterialCache.hpp"

namespace mirai
{
    MaterialCache *MaterialCache::Instance = nullptr;

    MaterialCache::MaterialCache()
    {
        Instance = this;
    }

    void MaterialCache::register_shader(uint32_t shader_hash, std::vector<ShaderID> shaders)
    {
        if (shader_caches.find(shader_hash) == shader_caches.end())
            shader_caches[shader_hash] = shaders;
    }

    std::vector<ShaderID> MaterialCache::get_shaders(uint32_t hash)
    {
        auto found = shader_caches.find(hash);
        if (found != shader_caches.end())
            return found->second;
        return {};
    }

    PipelineID MaterialCache::get_pipeline(uint64_t hash)
    {
        auto found = pipeline_caches.find(hash);
        if (found != pipeline_caches.end())
            return found->second;
        return PipelineID{K_INVALID_ID};
    }

    void MaterialCache::add_pipeline(uint64_t hash, PipelineID pipeline)
    {
        pipeline_caches[hash] = pipeline;
    }

    MaterialCache::~MaterialCache()
    {
        for (auto &[key, val] : shader_caches)
        {
            RenderingDevice::get()->destroy_shaders(val.data(), static_cast<uint32_t>(val.size()));
        }

        for (auto &[key, val] : pipeline_caches)
        {
            RenderingDevice::get()->destroy_pipeline(&val, 1);
        }
    }
} // namespace mirai