#pragma once
#include "RenderPass/RenderPass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "ImGuiService.hpp"
#include "Graphics/Renderer.hpp"

using namespace mirai;

struct ImGuiRenderPass {
  public:
    ImGuiRenderPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass(
            "ImGuiPass",
            [board](FrameGraph::FrameGraphBuilder &builder, FrameGraph::NoData &no_data) {
                const FinalCompositePassData &data = board->get<FinalCompositePassData>();
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           });
                builder.present(data.output);
            },
            [](const FrameGraph::NoData &no_data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;
                RenderingDevice *device = RenderingDevice::get();

                ScopedGpuProfiling(command_buffer, "ImGui Pass");

                device->begin_debug_utils_label(command_buffer, "ImGui Pass", nullptr);

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                FrameGraphBlackBoard *board = ctx->renderer->get_frame_graph_blackboard();
                const FinalCompositePassData &composite_output = board->get<FinalCompositePassData>();
                const FrameGraphTexture &texture = pass_resource.get<FrameGraphTexture>(composite_output.output);

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
                device->end_debug_utils_label(command_buffer);
            });
    }
};