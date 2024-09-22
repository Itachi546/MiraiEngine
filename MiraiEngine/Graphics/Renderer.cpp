#include "Renderer.hpp"

#include "RenderingDevice.hpp"
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

    void Renderer::compile_passes()
    {
    }

    void Renderer::update()
    {
        for (auto &scene_pass : scene_passes)
            scene_pass->update();
    }

    void Renderer::render()
    {
        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        for (auto &scene_pass : scene_passes)
            scene_pass->render(cb, scene.get());

        device->queue_command_buffer(cb);

        device->present();
    }

    Renderer::~Renderer()
    {
    }

} // namespace mirai