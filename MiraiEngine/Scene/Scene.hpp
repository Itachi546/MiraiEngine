#pragma once

#include "ECS.hpp"

#include <string>

namespace mirai
{
    struct ComponentManager;
    class CommandBuffer;

    class Scene
    {
      public:
        Scene(const std::string &name);

        void add_entity(Entity entity) { entities.push_back(entity); }

        void remove_entity(Entity entity) { ecs::destroy_entity(component_manager.get(), entity); }

        ComponentManager *get_component_manager() { return component_manager.get(); }

        void update();

        std::string get_name() const
        {
            return name;
        }

        void set_name(const std::string &name)
        {
            this->name = name;
        }

        const std::vector<Entity> &get_entities()
        {
            return entities;
        }

        virtual ~Scene() = default;

      protected:
        std::string name;
        std::unique_ptr<ComponentManager> component_manager;
        std::vector<Entity> entities;
    };
} // namespace mirai
