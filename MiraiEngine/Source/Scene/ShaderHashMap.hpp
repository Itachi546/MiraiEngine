#pragma once

#include "Graphics/RenderingDevice.hpp"
#include "Common/Hash.hpp"
#include "Common/HashMap.hpp"
#include <vector>

namespace mirai {
    struct Shader;

    class ShaderHashMap {

      public:
        ShaderHashMap();
        ShaderHashMap(const ShaderHashMap &) = delete;
        void operator=(const ShaderHashMap &) = delete;

        ~ShaderHashMap();

        Shader *add(uint64_t shader_key, std::shared_ptr<Shader> shader);
        Shader *get(uint64_t shader_key);

        static ShaderHashMap *get() {
            return Instance;
        }

        void destroy();

      private:
        static ShaderHashMap *Instance;
        HashMap<uint64_t, std::shared_ptr<Shader>> shader_map;
    };

} // namespace mirai