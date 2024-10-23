#include "ForwardPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    ForwardPass::ForwardPass(const std::string &name) : FrameGraphRenderPass(name)
    {
        shader = std::make_shared<ShaderMaterial>("TriangleMaterial");
        shader->create_from_file({
            "SPIRV/triangle.vert.spv",
            "SPIRV/triangle.frag.spv",
        });
        shader->set_cull_mode(CULL_MODE_NONE);
        shader->set_depth_write(true);
        shader->set_depth_test(true);
    }

    void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene)
    {
        ASSERT(node != nullptr);

        command_buffer->begin_render_pass(node, frame_graph);

        const Entity entity = scene->get_entities()[0];
        ComponentManager *component_manager = scene->get_component_manager();
        
        shader->bind(command_buffer, node, frame_graph);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
    }
} // namespace mirai