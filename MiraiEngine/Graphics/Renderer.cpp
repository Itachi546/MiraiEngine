#include "Renderer.hpp"

#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"

namespace mirai
{
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer()
    {
        device = std::make_unique<VulkanRenderingDevice>();
        Instance = this;
    }

    void Renderer::update()
    {
        for (auto &scene_pass : scene_passes)
            scene_pass->update();
    }

    void Renderer::render_scene_pass(CommandBuffer *cb, std::unique_ptr<ScenePass> &scene_pass)
    {
        cb->begin_render_pass(scene_pass->get_render_pass());
        scene_pass->update();
        cb->end_render_pass();
    }

    void Renderer::render()
    {
        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        for (auto &scene_pass : scene_passes)
            render_scene_pass(cb, scene_pass);

        device->queue_command_buffer(cb);

        device->present();
    }

    Renderer::~Renderer()
    {
    }

} // namespace mirai