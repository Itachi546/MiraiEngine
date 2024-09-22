#pragma once

#include <unordered_map>
#include <vector>

#include "RenderingDevice.hpp"

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

      private:
        static MaterialCache *Instance;

        std::unordered_map<uint32_t, std::vector<ShaderID>> shader_caches;
    };

}; // namespace mirai