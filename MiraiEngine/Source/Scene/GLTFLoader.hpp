#pragma once

#include "ECS.hpp"

namespace mirai {
    class Scene;

    Entity ImportModel_GLTF(const std::string &filename, Scene *scene);

} // namespace mirai