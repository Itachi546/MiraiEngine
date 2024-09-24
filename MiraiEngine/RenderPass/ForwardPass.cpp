#include "ForwardPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Material.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    ForwardPass::ForwardPass()
    {
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, Scene *scene)
    {
        FrameGraphNode *node = frame_graph->get_node("forward_pass");
        ASSERT(node != nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        const Entity entity = scene->get_entities()[0];
        ComponentManager *component_manager = scene->get_component_manager();
        Material *material = component_manager->get_component<Material>(entity);
        material->bind(command_buffer, &node->render_pass);

        command_buffer->draw(6, 1, 0, 0);

        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
    }
} // namespace mirai