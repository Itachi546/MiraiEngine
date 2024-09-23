#include "Renderer.hpp"

#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/MaterialCache.hpp"

namespace mirai
{
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer()
    {
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        material_cache = std::make_unique<MaterialCache>();
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
        device->wait();
    }

} // namespace mirai