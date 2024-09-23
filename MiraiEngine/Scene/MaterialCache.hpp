#pragma once

#include <unordered_map>
#include <vector>

#include "Graphics/RenderingDevice.hpp"

namespace mirai
{
    class MaterialCache
    {
      public:
        MaterialCache();

        static MaterialCache *get()
        {
            return Instance;
        }

        void register_shader(uint32_t shader_hash, std::vector<ShaderID> shaders);

        std::vector<ShaderID> get_shaders(uint32_t hash);

        PipelineID get_pipeline(uint64_t hash);

        void add_pipeline(uint64_t hash, PipelineID pipeline);

        ~MaterialCache();

      private:
        static MaterialCache *Instance;

        std::unordered_map<uint32_t, std::vector<ShaderID>> shader_caches;
        std::unordered_map<uint64_t, PipelineID> pipeline_caches;
    };

}; // namespace mirai