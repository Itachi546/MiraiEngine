#include "FullScreenPass.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"

#include <string>

namespace mirai
{

    FullScreenPass::FullScreenPass(const std::string &name) : FrameGraphRenderPass(name)
    {
        material = std::make_shared<ShaderMaterial>("FullScreenTextureMaterial");
        material->create_from_file(std::vector<std::string>{
            "SPIRV/fullscreen.vert.spv",
            "SPIRV/fullscreen.frag.spv",
        });
        material->set_front_face(FRONT_FACE_CLOCKWISE);
    }

    void FullScreenPass::initialize(FrameGraph *frame_graph, const FrameGraphNode* node)
    {
        FrameGraphResource *input_texture = frame_graph->get_resource(node->inputs[0]);
        material->set_resource("u_texture", input_texture->texture);
    }

    void FullScreenPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, const FrameGraphNode* node, Scene *scene)
    {
        ASSERT(node != nullptr);

        uint32_t width, height;
        Window::get()->get_size(&width, &height);
        set_size(width, height);

        float push_constants[] = {(float)width, (float)height, static_cast<float>(enable_aa)};

        command_buffer->begin_render_pass(node, frame_graph);

        material->bind(command_buffer, node, frame_graph);
        material->set_push_constant(command_buffer,
                                    SHADER_STAGE_FRAGMENT,
                                    0,
                                    static_cast<uint32_t>(sizeof(float) * 3),
                                    push_constants);

        command_buffer->draw(6, 1, 0, 0);

        command_buffer->end_render_pass();
    }

    FullScreenPass::~FullScreenPass()
    {
    }

} // namespace mirai