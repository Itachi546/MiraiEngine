#include "Renderer.hpp"

#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterialCache.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "Device/Window.hpp"
#include <glm/gtc/matrix_transform.hpp>

namespace mirai
{
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer(bool enable_validation)
    {
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>(enable_validation);
        scene = std::make_unique<Scene>("default");
        material_cache = std::make_unique<ShaderMaterialCache>();
        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());

        BufferDescription buffer_desc = {
            .size = sizeof(FrameData),
            .usage_flags = BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        per_frame_data_buffer = device->create_buffer(&buffer_desc, "per_frame_data_buffer");
        frame_data_ptr = (FrameData *)device->map_buffer(per_frame_data_buffer);
    }

    void Renderer::compile_passes()
    {
    }

    void Renderer::update()
    {
        uint32_t width, height;
        Window::get()->get_size(&width, &height);
        frame_data_ptr->elapsed_time = Engine::get()->get_elapsed_seconds();
        frame_data_ptr->P = glm::perspective(glm::radians(60.0f), float(width) / float(height), 0.1f, 500.0f);
        frame_data_ptr->V = glm::lookAt(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        frame_data_ptr->window_size = glm::vec2((float)width, (float)height);

        scene->update();
    }

    void Renderer::render()
    {
        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        frame_graph->render(cb, scene.get());

        device->queue_command_buffer(cb);

        device->present();
    }

    Renderer::~Renderer()
    {
        device->destroy_buffers(&per_frame_data_buffer, 1);
        device->wait();
        scene.reset();
    }

} // namespace mirai