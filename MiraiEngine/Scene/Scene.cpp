#include "Scene.hpp"
#include "Material.hpp"
#include "Component.hpp"

namespace mirai
{
    Scene::Scene(const std::string &name) : name(name)
    {
        component_manager = std::make_unique<ComponentManager>();
        component_manager->register_component<Material>();
        component_manager->register_component<NameComponent>();
    }

    void Scene::update()
    {
    }
} // namespace mirai