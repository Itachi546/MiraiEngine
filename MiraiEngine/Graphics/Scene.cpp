#include "Scene.hpp"

namespace mirai
{
    Scene::Scene()
    {
        component_manager = std::make_unique<ComponentManager>();
    }

    void Scene::update()
    {
    }
} // namespace mirai