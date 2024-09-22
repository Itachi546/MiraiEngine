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
} // namespace mirai