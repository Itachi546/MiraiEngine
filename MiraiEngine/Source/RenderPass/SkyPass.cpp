#include "SkyPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"

namespace mirai {
    SkyPass::SkyPass() : FrameGraphRenderPass("sky_pass") {
        material = std::make_shared<ProceduralSkyMaterial>();
        material->set_depth_test(true);
        material->set_depth_write(false);
        material->set_depth_compare_op(COMPARE_OP_EQUAL);
    }

    void SkyPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        RenderingDevice::get()->begin_debug_utils_label(command_buffer, "SkyShader", nullptr);
        ScopedGpuProfiling(command_buffer, "Sky");

        Camera *camera = scene->get_camera();
        material->set_inv_projection_matrix(camera->get_inv_projection_transform());
        material->set_inv_view_matrix(camera->get_inv_view_transform());

        command_buffer->begin_render_pass(node, frame_graph);

        material->bind(command_buffer, node, frame_graph);

        command_buffer->draw(3, 1, 0, 0);

        command_buffer->end_render_pass();

        RenderingDevice::get()->end_debug_utils_label(command_buffer);
    }
} // namespace mirai