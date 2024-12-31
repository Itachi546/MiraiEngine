#include "Overlay3DPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Scene/SkyMaterial.hpp"
#include "Scene/EnvironmentMap.hpp"

namespace mirai {
    Overlay3DPass::Overlay3DPass() : FrameGraphRenderer("sky_pass"), skybox_material(nullptr) {
    }

    void Overlay3DPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        skybox_material = std::make_shared<SkyboxMaterial>();
        skybox_material->set_depth_test(true);
        skybox_material->set_depth_write(false);
        skybox_material->set_depth_compare_op(COMPARE_OP_EQUAL);

        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
            .shader_stage = SHADER_STAGE_FRAGMENT,
        };
        skybox_uniform_set = device->create_uniform_set(&layout, 1, 0, "skybox_binding");
    }

    void Overlay3DPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "Overlay3D", nullptr);
        ScopedGpuProfiling(command_buffer, "Overlay3D");

        command_buffer->begin_render_pass(node, frame_graph);

        if (scene->get_environment_map() != nullptr)
            render_skybox(command_buffer, scene, &node->renderpass_info);

        render_debug_draw(command_buffer, scene, &node->renderpass_info);

        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    void Overlay3DPass::render_skybox(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass) {
        Camera *camera = scene->get_camera();
        skybox_material->set_inv_projection_matrix(camera->get_inv_projection_transform());

        TextureID skybox = scene->get_environment_map()->get_cubemap();
        UniformBinding binding = {.resource_id = skybox};
        device->update_uniform_set(skybox_uniform_set, &binding, 1);

        // Draw Sky
        skybox_material->set_uniform_sets(&skybox_uniform_set, 1);
        skybox_material->set_inv_view_matrix(camera->get_inv_view_transform());
        skybox_material->bind(command_buffer, render_pass);
        command_buffer->draw(3, 1, 0, 0);
    }

    void Overlay3DPass::render_debug_draw(CommandBuffer *command_buffer, Scene *scene, FrameGraphRenderpassInfo *render_pass) {
        // Draw Lines
        LineRenderer *line_renderer = LineRenderer::get();
        if (line_renderer->line_count > 0) {
            Camera *camera = scene->get_camera();
            glm::mat4 VP = camera->get_view_projection_transform();
            PushConstant push_constant = {
                .data = &VP[0][0],
                .shader_stage = SHADER_STAGE_VERTEX,
                .size = sizeof(glm::mat4),
                .offset = 0,
            };

            line_renderer->shader_material->set_push_constant(&push_constant, 1);
            line_renderer->shader_material->bind(command_buffer, render_pass);

            command_buffer->draw(line_renderer->line_count * 2, 1, 0, 0);
        }
    }
} // namespace mirai