#include "ECS.hpp"

namespace mirai {

    void ECS::destroy_entity(Entity &entity) {
        for (uint32_t i = 0; i < K_MAX_COMPONENTS; ++i) {
            std::shared_ptr<IComponentArray> comp = component_manager->get_base_component_array(i);
            if (comp)
                comp->remove_entity(entity);
        }
        entity = 0;
    }

    void ECS::destroy() {
        for (uint32_t i = 0; i < K_MAX_COMPONENTS; ++i) {
            std::shared_ptr<IComponentArray> comp = component_manager->get_base_component_array(i);
            comp.reset();
        }
        component_manager = nullptr;
    }

} // namespace mirai