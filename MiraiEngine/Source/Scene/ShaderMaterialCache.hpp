#pragma once

#include <unordered_map>
#include <vector>

#include "Graphics/RenderingDevice.hpp"

namespace mirai {
    class ShaderMaterialCache {
      public:
        ShaderMaterialCache();

        static ShaderMaterialCache *get() {
            return Instance;
        }

        void cache_shader(uint32_t shader_hash, ShaderID shader);

        ShaderID get_shader(uint32_t hash);

        PipelineID get_pipeline(uint64_t hash);

        void add_pipeline(uint64_t hash, PipelineID pipeline);

        ~ShaderMaterialCache();

      private:
        static ShaderMaterialCache *Instance;

        std::unordered_map<uint32_t, ShaderID> shader_caches;
        std::unordered_map<uint64_t, PipelineID> pipeline_caches;
    };

}; // namespace mirai