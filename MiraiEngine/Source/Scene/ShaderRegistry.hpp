#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include "Shader.hpp"

#include <vector>
#include <atomic>

namespace mirai {

    class ShaderRegistry {
      public:
        ShaderRegistry();

        ShaderRegistry(const ShaderRegistry &) = delete;
        ShaderRegistry(ShaderRegistry &&) = delete;
        void operator=(const ShaderRegistry &) = delete;
        void operator=(ShaderRegistry &&) = delete;

        ~ShaderRegistry() = default;

        Shader *find(uint32_t pso_key) {
            auto found = table.find(pso_key);
            if (found == table.end())
                return nullptr;
            return found->second.get();
        }

        bool has(uint32_t pso_key) {
            return table.find(pso_key) != table.end();
        }

        void add(uint64_t pso_key, std::shared_ptr<Shader> shader) {
            auto found = table.find(pso_key);
            if (found == table.end())
                table.insert(std::make_pair(pso_key, shader));
            else {
                ASSERT("Failed to add sortkey in registry, already exists");
            }
        }

        static ShaderRegistry *get() {
            return Instance;
        }

        void destroy();

      private:
        static ShaderRegistry *Instance;
        HashMap<uint64_t, std::shared_ptr<Shader>> table;
    };

} // namespace mirai