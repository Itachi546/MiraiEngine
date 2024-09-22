#include "ForwardPass.hpp"

#include "Graphics/RenderingDevice.hpp"
#include "Graphics/Scene.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai
{
    ForwardPass::ForwardPass(uint32_t width, uint32_t height)
    {
        std::vector<Attachment> color_attachments = {
            Attachment{0, COLOR_ATTACHMENT_OUTPUT, ATTACHMENT_TYPE_SWAPCHAIN, FORMAT_UNDEFINED, {0.0f, 0.0f, 0.0f, 1.0f}},
        };

        render_pass = std::make_unique<RenderPass>();
        render_pass->color_attachments = color_attachments;
        render_pass->depth_attachments = {};
        render_pass->width = width;
        render_pass->height = height;
    }

    void ForwardPass::update()
    {
    }

    void ForwardPass::render(CommandBuffer *command_buffer, Scene *scene)
    {
        command_buffer->begin_render_pass(render_pass.get());

        command_buffer->end_render_pass();
    }

    ForwardPass::~ForwardPass()
    {
    }
} // namespace mirai