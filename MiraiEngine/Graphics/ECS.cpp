#include "ECS.hpp"

namespace mirai::ecs
{
    void destroy_entity(ComponentManager *mgr, Entity &entity)
    {
        for (uint32_t i = 0; i < MAX_COMPONENTS; ++i)
        {
            std::shared_ptr<IComponentArray> comp = mgr->get_base_component_array(i);
            if (comp)
                comp->remove_entity(entity);
        }
        entity = 0;
    }

    void destroy(ComponentManager *mgr)
    {
        for (uint32_t i = 0; i < MAX_COMPONENTS; ++i)
        {
            std::shared_ptr<IComponentArray> comp = mgr->get_base_component_array(i);
            comp.reset();
        }
    }

} // namespace mirai::ecs