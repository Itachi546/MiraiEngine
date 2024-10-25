#include "Scene.hpp"
#include "ShaderMaterial.hpp"
#include "Component.hpp"

#include <execution>
#include <algorithm>

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
        component_manager->register_component<MeshDataComponent>();
    }

    void Scene::update()
    {
        update_transform_components();
    }

    void Scene::remove_entity_tree(Entity entity)
    {
        if (component_manager->has_component<HierarchyComponent>(entity))
        {
            HierarchyComponent *comp = component_manager->get_component<HierarchyComponent>(entity);
            for (auto child : comp->childrens)
                remove_entity_tree(child);
        }
        ecs::destroy_entity(component_manager.get(), entity);
    }

    void Scene::update_transform_components()
    {
        auto transform_array_ptr = component_manager->get_component_array<TransformComponent>();
        std::vector<TransformComponent> &transforms = transform_array_ptr->components;
        std::for_each(std::execution::par_unseq,
                      transforms.begin(),
                      transforms.end(),
                      [](TransformComponent &transform)
                      { transform.update_local_transform(); });
    }

    void Scene::remove_entity(Entity entity)
    {
        auto found = std::find(entities.begin(), entities.end(), entity);
        if (found == entities.end())
        {
            Log::Warn("Entity doesn't belong to the scene");
            return;
        }

        remove_entity_tree(entity);
        entities.erase(found);
    }

    void Scene::release_all_entities()
    {
        for (auto entity : entities)
            remove_entity_tree(entity);
        entities.clear();
    }

    Scene::~Scene()
    {
        release_all_entities();

        for (auto &comp_array : component_manager->component_array)
        {
            if (comp_array)
                ASSERT(comp_array->size() == 0);
        }
        ecs::destroy(component_manager.get());
    }

} // namespace mirai