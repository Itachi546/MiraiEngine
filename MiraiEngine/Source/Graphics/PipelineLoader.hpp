#pragma once

#include <string>

namespace mirai {
    class ShaderHashMap;
    void preload_shaders(ShaderHashMap *pipeline_hashmap, const std::string& filename);
} // namespace mirai