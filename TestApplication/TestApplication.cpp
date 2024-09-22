#include "Engine/Engine.hpp"
#include "Engine/App.hpp"
#include "Engine/Log.hpp"
#include "Device/Window.hpp"
#include "Device/InputDevice.hpp"
#include "Common/Color.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Material.hpp"

#include <fstream>
#include <filesystem>

using namespace mirai;
const Color Color_Black = {0.0f, 0.0f, 0.0f, 1.0f};

class TestApplication : public App
{
  public:
    TestApplication() : App("TestApplication")
    {
        Window::get()->set_title("TestApplication");
    }

    void start() override
    {
        std::vector<Attachment> color_attachments = {
            Attachment{0, "main_color_attachment", ATTACHMENT_TYPE_SWAPCHAIN, FORMAT_UNDEFINED, Color_Black},
        };

        int width, height;
        Window::get()->get_size(&width, &height);

        RenderPass main_render_pass = {
            .color_attachments = color_attachments,
            .depth_attachments = {},
            .width = uint32_t(width),
            .height = uint32_t(height),
        };

        ShaderID shaders[] = {
            rendering_utils::create_shader_module_from_file("SPIRV/main.vert.spv"),
            rendering_utils::create_shader_module_from_file("SPIRV/main.frag.spv"),
        };

        RasterizationState rs = RasterizationState::create();
        DepthState ds = DepthState::create();
        BlendState bs = BlendState::create();
        Format format = FORMAT_B8G8R8A8_UNORM;

        PipelineDescription pipeline_description;
        pipeline_description.shader_count = 2;
        pipeline_description.shaders = shaders;
        pipeline_description.rasterization_state = &rs;
        pipeline_description.depth_state = &ds;
        pipeline_description.blend_state = &bs;
        pipeline_description.color_attachment_count = 1;
        pipeline_description.color_attachment_formats = &format;

        PipelineID pipeline = RenderingDevice::get()->create_graphics_pipeline(&pipeline_description, "test_pipline");

        RenderingDevice::get()->destroy_shaders(shaders, 2);
        RenderingDevice::get()->destroy_pipeline(&pipeline, 1);
    }

    void update() override
    {
        float time = Engine::get()->get_elapsed_seconds();
        if (Input::get()->is_down(KB_ESCAPE))
            Engine::get()->request_close();
    }

    ~TestApplication()
    {
        Log::Info("Destroying Test Application...");
    }
};

int main()
{
    std::unique_ptr<Engine> engine = std::make_unique<Engine>();
    engine->set_app(std::make_unique<TestApplication>());
    engine->run();
    engine = nullptr;
}