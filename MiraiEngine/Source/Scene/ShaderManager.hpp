#pragma once

#include "Graphics/RenderingDevice.hpp"
#include <unordered_map>

namespace mirai {
    class ShaderMaterial;
    struct ShaderMaterialProperties;

    class ShaderManager {

      public:
        ShaderManager();
        ~ShaderManager();

        ShaderMaterial *load(const std::string &name, const std::vector<std::string> &shaders, const ShaderMaterialProperties &props);
        // ShaderMaterial* load(const std::string &name, std::shared_ptr<ShaderMaterial> material);
        ShaderMaterial *get_shader(const std::string &name);

        static ShaderManager *get() {
            return Instance;
        }

      private:
        static ShaderManager *Instance;
        std::unordered_map<uint32_t, std::shared_ptr<ShaderMaterial>> shader_material_cache;
    };

} // namespace mirai