#include "Scene.hpp"
#include "ShaderMaterial.hpp"
#include "Component.hpp"

namespace mirai
{
    Scene::Scene(const std::string &name) : name(name)
    {
        component_manager = std::make_unique<ComponentManager>();
        component_manager->register_component<MaterialComponent>();
        component_manager->register_component<NameComponent>();
        component_manager->register_component<HierarchyComponent>();
        component_manager->register_component<MeshComponent>();
        component_manager->register_component<TransformComponent>();
        component_manager->register_component<ObjectComponent>();
    }

    void Scene::update()
    {
    }
} // namespace mirai