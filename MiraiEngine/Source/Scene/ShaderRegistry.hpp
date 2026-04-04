#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include <vector>

namespace mirai {
    struct Shader;

    enum PassMode {
        PASS_MODE_DEPTH_PREPASS,
    };

    struct ShaderRegistry {
        std::string name;
        HashMap<uint32_t, std::shared_ptr<Shader>> table;
    };

    class ShaderRegistryMap {
      public:
        ShaderRegistryMap();
        ShaderRegistryMap(const ShaderRegistryMap &) = delete;
        ShaderRegistryMap(ShaderRegistryMap &&) = delete;
        void operator=(const ShaderRegistryMap &) = delete;
        void operator=(ShaderRegistryMap &&) = delete;

        ~ShaderRegistryMap() = default;

        ShaderRegistry *add_registry(PassMode pass_mode, std::shared_ptr<ShaderRegistry> shader);
        ShaderRegistry *get_registry(PassMode pass_mode);

        static ShaderRegistryMap *get() {
            return Instance;
        }

        void destroy();

      private:
        static ShaderRegistryMap *Instance;
        HashMap<uint64_t, std::shared_ptr<ShaderRegistry>> shader_registry_map;
    };

} // namespace mirai