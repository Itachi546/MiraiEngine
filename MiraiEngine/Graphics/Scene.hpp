#pragma once

#include "ECS.hpp"

namespace mirai
{
    struct ComponentManager;
    class CommandBuffer;

    class Scene
    {
      public:
        Scene();

        void add_entity(Entity entity) { entities.push_back(entity); }

        void remove_entity(Entity entity) { ecs::destroy_entity(component_manager.get(), entity); }

        ComponentManager *get_component_manager() { return component_manager.get(); }

        void update();

        virtual ~Scene() = default;

      protected:
        std::unique_ptr<ComponentManager> component_manager;
        std::vector<Entity> entities;
    };
} // namespace mirai
