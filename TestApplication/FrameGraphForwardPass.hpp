#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "ImGuiService.hpp"
#include "Graphics/GPUResource.hpp"
#include "Scene/Material.hpp"
#include "DebugTexturePass.hpp"

using namespace mirai;

#define ENABLE_RT_PASS 0

void initialize_forward_pass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
    RTGroundTruthPass rt_ground_truth{frame_graph, board};
    DepthPrePass depth_prepass{frame_graph, board};
    ViewNormalDepthPass view_pass{frame_graph, board};
    DDGIPass ddgi_pass{frame_graph, board};
    SSAOPass ssao_pass{frame_graph, board};
    CascadedShadowPass cascaded_shadow_pass{frame_graph, board};
    TiledLightCullingPass light_cull_pass{frame_graph, board};
    ForwardPass forward_pass{frame_graph, board};
    BloomPass bloom_pass{frame_graph, board};
    TAAResolvePass taa_pass{frame_graph, board};
    FinalCompositePass composite_pass{frame_graph, board};

    // ImGui Pass
    struct ImGuiPassData {
        FrameGraphResourceHandle output;
    };

    frame_graph->add_callback_pass<ImGuiPassData>(
        "ImGuiPass",
        [board](FrameGraph::Builder &builder, ImGuiPassData &data) {
            const FinalCompositePassData &input_pass = board->get<FinalCompositePassData>();
            data.output = input_pass.output;
            builder.write(data.output, {
                                           .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE | ACCESS_FLAG_COLOR_ATTACHMENT_READ,
                                           .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

            builder.present(data.output);
        },
        [](const ImGuiPassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;

            ScopedGpuProfiling(command_buffer, "ImGui Pass");
            command_buffer->begin_gpu_debug_label("ImGuiPass");

            const auto &resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            uint32_t width, height;
            Window::get()->get_size(&width, &height);

            const FrameGraphTexture &texture = pass_resource.get<FrameGraphTexture>(data.output);
            command_buffer->begin_render_pass({
                                                  AttachmentInfo{
                                                      .texture = texture.id,
                                                      .load_op = LOAD_OP_LOAD,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                                                  },
                                              },
                                              std::nullopt, width, height);

            command_buffer->set_viewport({
                .x = 0.0f,
                .y = 0.0f,
                .width = cast_float(width),
                .height = cast_float(height),
                .min_depth = 0.0,
                .max_depth = 1.0,
            });

            command_buffer->set_scissor(0, 0, width, height);

            ImGuiService::Render(command_buffer);

            command_buffer->end_render_pass();
            command_buffer->end_gpu_debug_label();
        });
}