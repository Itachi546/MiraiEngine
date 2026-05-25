#pragma once

#include "Common/CommonInclude.hpp"
#include "Common/Log.hpp"

#include <vector>
#include <array>
#include <memory>
#include "Common/HashMap.hpp"
#include <algorithm>

namespace mirai {

    constexpr uint32_t K_INVALID_ENTITY = 0;
    using ComponentType = std::uint8_t;
    const uint8_t K_MAX_COMPONENTS = 64;

    using Entity = std::uint32_t;

    inline bool is_valid(Entity entity) {
        return entity != K_INVALID_ENTITY;
    }
    /*
     * This doesn't seem to work when the dll is made and
     * called from exe?
     */
    static inline uint32_t GetId() {
        static uint32_t g_component_id = 0;
        ASSERT_MSG(g_component_id < K_MAX_COMPONENTS, "Component count exceed the max limit");
        return g_component_id++;
    }

    class IComponentArray {
      public:
        virtual ~IComponentArray() = default;

        virtual bool remove_entity(Entity &) { return false; }

        virtual std::size_t size() const = 0;
    };

    template <typename T>
    class ComponentArray : public IComponentArray {
      public:
        ComponentArray() {}

        // Remove copy constructor and copy-assignment constructor
        ComponentArray(const ComponentArray &) = delete;
        void operator=(const ComponentArray &) = delete;

        T &add_component(Entity entity) {
            ASSERT(components.size() == entities.size());
            ASSERT(entities.size() == lookup_.size());
            ASSERT(is_valid(entity));

            auto found = lookup_.find(entity);
            if (found != lookup_.end())
                return components[found->second];

            lookup_[entity] = static_cast<uint32_t>(components.size());
            components.emplace_back();
            entities.push_back(entity);

            return components.back();
        }

        template <typename... Args>
        T &add_component(Entity entity, Args &&...args) {
            ASSERT(components.size() == entities.size());
            ASSERT(entities.size() == lookup_.size());

            auto found = lookup_.find(entity);
            if (found != lookup_.end())
                return components[found->second];

            lookup_[entity] = static_cast<uint32_t>(components.size());
            components.push_back(T{std::forward<Args>(args)...});
            entities.push_back(entity);
            return components.back();
        }

        bool remove_component(Entity entity) {
            auto found = lookup_.find(entity);
            if (found != lookup_.end()) {
                uint64_t index = found->second;
                components[index] = std::move(components.back());
                entities[index] = entities.back();
                lookup_[entities[index]] = static_cast<uint32_t>(index);
                lookup_.erase(entity);
                components.pop_back();
                entities.pop_back();
                return true;
            }
            return false;
        }

        bool remove_entity(Entity &entity) override {
            return remove_component(entity);
        }

        T *get_component(Entity entity) {
            auto found = lookup_.find(entity);
            if (found != lookup_.end())
                return &components[found->second];
            return nullptr;
        }

        uint32_t get_component_index(Entity entity) {
            auto found = lookup_.find(entity);
            if (found != lookup_.end())
                return found->second;
            return UINT32_MAX;
        }

        std::size_t size() const override {
            return components.size();
        }

        std::vector<Entity> entities;
        std::vector<T> components;

      private:
        HashMap<uint32_t, uint32_t> lookup_;
    };

    struct ComponentManager {
        ComponentManager() = default;
        ComponentManager(const ComponentManager &) = delete;
        void operator=(const ComponentManager &) = delete;

        template <typename T>
        void register_component() {
            std::size_t comp_hash = typeid(T).hash_code();
            auto found = component_id_map.find(comp_hash);
            if (found != component_id_map.end())
                return;
            uint32_t id = GetId();
            component_array[id] = std::make_shared<ComponentArray<T>>();
            component_id_map.insert(std::make_pair(comp_hash, id));
        }

        template <typename T>
        uint32_t get_component_type_id() {
            std::size_t comp_hash = typeid(T).hash_code();
            auto found = component_id_map.find(comp_hash);
            ASSERT(found != component_id_map.end());
            return found->second;
        }

        template <typename T>
        inline std::shared_ptr<ComponentArray<T>> get_component_array() {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);
            return std::static_pointer_cast<ComponentArray<T>>(component_array[comp_id]);
        }

        template <typename T>
        inline std::shared_ptr<ComponentArray<T>> get_component_array_by_index(int index) {
            return std::static_pointer_cast<ComponentArray<T>>(component_array[index]);
        }

        inline std::shared_ptr<IComponentArray> get_base_component_array(uint64_t index) {
            ASSERT(index < K_MAX_COMPONENTS);

            return component_array[index];
        }

        template <typename T>
        bool has_component(const Entity &entity) {
            uint32_t comp_id = get_component_type_id<T>();
            auto comp = get_component_array_by_index<T>(comp_id);
            return comp->get_component_index(entity) != UINT32_MAX;
        }

        /*
         * We should be careful when keeping this reference for future use.
         * When the size of vector is not enough, the container is resized
         * which invalidate all the reference and the pointer.
         */
        template <typename T>
        T &add_component(Entity &entity) {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);
            auto comp = get_component_array_by_index<T>(comp_id);
            ASSERT(comp != nullptr);
            return comp->add_component(entity);
        }

        template <typename T, typename... Args>
        T &add_component(Entity &entity, Args &&...args) {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);
            auto comp = get_component_array<T>();
            ASSERT(comp != nullptr);
            return comp->add_component(entity, std::forward<Args>(args)...);
        }

        template <typename T>
        T *get_component(const Entity &entity) {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);

            auto comp = get_component_array_by_index<T>(comp_id);
            ASSERT(comp != nullptr);
            return comp->get_component(entity);
        }

        template <typename T>
        uint32_t get_component_index(const Entity &entity) {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);

            auto comp = get_component_array_by_index<T>(comp_id);
            ASSERT(comp != nullptr);
            return comp->get_component_index(entity);
        }

        template <typename T>
        bool remove_component(Entity &entity) {
            uint32_t comp_id = get_component_type_id<T>();
            ASSERT(comp_id < K_MAX_COMPONENTS);
            auto comp = get_component_array<T>(comp_id);
            ASSERT(comp != nullptr);
            return comp->remove_component(entity);
        }

        // Global Component Array
        std::array<std::shared_ptr<IComponentArray>, K_MAX_COMPONENTS> component_array;
        HashMap<std::size_t, uint32_t> component_id_map;
    };

    class ECS {
      public:
        ECS() : id(0) {
            component_manager = std::make_unique<ComponentManager>();
        }

        Entity create_entity() {
            return ++id;
        }

        void destroy_entity(Entity &entity);
        void destroy();

        std::unique_ptr<ComponentManager> component_manager;

      private:
        uint32_t id;
    };
} // namespace mirai