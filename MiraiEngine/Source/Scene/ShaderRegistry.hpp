#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include <vector>

namespace mirai {
    struct Shader;

    enum PassMode {
        PASS_MODE_DEPTH_PREPASS = 0,
        PASS_MODE_COUNT
    };

    static uint32_t GetCustomPassID() {
        static uint32_t pass_id = PASS_MODE_COUNT;
        return pass_id++;
    }

    struct ShaderRegistry {
        std::string name;
        HashMap<uint32_t, std::shared_ptr<Shader>> table;

        ShaderRegistry(const std::string_view name) : name(name) {}
        Shader *find(uint32_t sort_key) {
            auto found = table.find(sort_key);
            if (found == table.end())
                return nullptr;
            return found->second.get();
        }

        void add(uint32_t sort_key, std::shared_ptr<Shader> shader) {
            auto found = table.find(sort_key);
            if (found == table.end())
                table.insert(std::make_pair(sort_key, shader));
            else {
                ASSERT("Failed to add sortkey in registry, already exists");
            }
        }
    };

    class ShaderRegistryMap {
      public:
        ShaderRegistryMap();
        ShaderRegistryMap(const ShaderRegistryMap &) = delete;
        ShaderRegistryMap(ShaderRegistryMap &&) = delete;
        void operator=(const ShaderRegistryMap &) = delete;
        void operator=(ShaderRegistryMap &&) = delete;

        ~ShaderRegistryMap() = default;

        ShaderRegistry *add_registry(uint32_t pass_mode, std::shared_ptr<ShaderRegistry> shader);
        ShaderRegistry *get_registry(uint32_t pass_mode);

        static ShaderRegistryMap *get() {
            return Instance;
        }

        void destroy();

      private:
        static ShaderRegistryMap *Instance;
        HashMap<uint64_t, std::shared_ptr<ShaderRegistry>> shader_registry_map;
    };

} // namespace mirai