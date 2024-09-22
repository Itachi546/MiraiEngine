#include "Scene.hpp"

namespace mirai
{
    Scene::Scene()
    {
        component_manager = std::make_unique<ComponentManager>();
    }
} // namespace mirai