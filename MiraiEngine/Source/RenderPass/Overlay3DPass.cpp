#include "Overlay3DPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {
    Overlay3DPass::Overlay3DPass() : FrameGraphRenderer("sky_pass"), skybox_material(nullptr) {
    }

    void Overlay3DPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        skybox_material = ShaderManager::get()->get_shader("overlay_skybox");

        SamplerDescription sampler_desc = SamplerDescription::create();
        default_sampler = device->create_sampler(&sampler_desc);

        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        skybox_uniform_set = device->create_uniform_set(&layout, 1, 0, "skybox_binding");
        TextureID skybox = renderer->get_scene()->get_environment_map()->get_cubemap();
        UniformBinding binding = {.resource_id = skybox, .texture_info = {.sampler = default_sampler}};
        device->update_uniform_set(skybox_uniform_set, &binding, 1);

        line_renderer = std::make_unique<LineRenderer>();
    }

    void Overlay3DPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
        line_renderer->NewFrame();
    }

    void Overlay3DPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
        device->begin_debug_utils_label(command_buffer, "Overlay3D", nullptr);
        ScopedGpuProfiling(command_buffer, "Overlay3D");

        command_buffer->begin_render_pass(node, frame_graph);

        Scene *scene = renderer->get_scene();
        if (scene->get_environment_map() != nullptr)
            render_skybox(command_buffer, scene, &node->renderpass_info);

        render_debug_draw(command_buffer, scene, &node->renderpass_info);

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    void Overlay3DPass::render_skybox(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass) {
        Camera *camera = scene->get_camera();

        // Draw Sky
        skybox_material->bind(command_buffer, render_pass);

        PipelineID pipeline_id = skybox_material->get_pipeline_id();
        command_buffer->set_uniform_sets(pipeline_id, &skybox_uniform_set, 1);

        glm::mat4 push_constant_data[] = {camera->get_inv_projection_transform(), camera->get_inv_view_transform()};
        PushConstant push_constant = {.data = push_constant_data, .shader_stage = SHADER_STAGE_FRAGMENT, .size = sizeof(glm::mat4) * 2, .offset = 0};

        command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
        command_buffer->draw(3, 1, 0, 0);
    }

    void Overlay3DPass::render_debug_draw(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass) {
        // Draw Lines
        if (line_renderer->line_count > 0) {
            Camera *camera = scene->get_camera();
            glm::mat4 VP = camera->get_view_projection_transform();
            PushConstant push_constant = {
                .data = &VP[0][0],
                .shader_stage = SHADER_STAGE_VERTEX,
                .size = sizeof(glm::mat4),
                .offset = 0,
            };

            line_renderer->shader_material->bind(command_buffer, render_pass);
            command_buffer->set_push_constants(line_renderer->shader_material->get_pipeline_id(), &push_constant, 1);

            command_buffer->draw(line_renderer->line_count * 2, 1, 0, 0);
        }
    }
} // namespace mirai