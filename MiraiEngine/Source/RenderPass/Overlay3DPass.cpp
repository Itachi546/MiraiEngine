#include "Overlay3DPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Scene/SkyMaterial.hpp"

namespace mirai {
    Overlay3DPass::Overlay3DPass() : FrameGraphRenderPass("sky_pass"), material(nullptr) {
    }

    void Overlay3DPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
        material = std::make_shared<ProceduralSkyMaterial>();
        material->set_depth_test(true);
        material->set_depth_write(false);
        material->set_depth_compare_op(COMPARE_OP_EQUAL);
    }

    void Overlay3DPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "Overlay3D", nullptr);
        ScopedGpuProfiling(command_buffer, "Overlay3D");

        Camera *camera = scene->get_camera();
        material->set_inv_projection_matrix(camera->get_inv_projection_transform());

        command_buffer->begin_render_pass(node, frame_graph);

        // Draw Sky
        material->set_inv_view_matrix(camera->get_inv_view_transform());
        material->bind(command_buffer, node, frame_graph);
        command_buffer->draw(3, 1, 0, 0);

        // Draw Lines
        LineRenderer *line_renderer = LineRenderer::get();
        if (line_renderer->line_count > 0) {
            glm::mat4 VP = camera->get_view_projection_transform();
            PushConstant push_constant = {
                .data = &VP[0][0],
                .shader_stage = SHADER_STAGE_VERTEX,
                .size = sizeof(glm::mat4),
                .offset = 0,
            };

            line_renderer->shader_material->set_push_constant(&push_constant, 1);
            line_renderer->shader_material->bind(command_buffer, node, frame_graph);

            command_buffer->draw(line_renderer->line_count * 2, 1, 0, 0);
        }
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }
} // namespace mirai