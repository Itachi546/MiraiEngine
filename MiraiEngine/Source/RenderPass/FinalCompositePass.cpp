#include "FinalCompositePass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {
    FinalCompositePass::FinalCompositePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<FinalCompositePassData>(
            "FinalCompositePass",
            [board](FrameGraph::FrameGraphBuilder &builder, FinalCompositePassData &data) {
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                data.output = builder.create_texture(
                    "FinalCompositeTexture",
                    {
                        .create_flags = 0,
                        .width = width,
                        .height = height,
                        .depth = 1,
                        .mip_levels = 1,
                        .array_layers = 1,
                        .texture_type = TEXTURE_TYPE_2D,
                        .format = FORMAT_B8G8R8A8_UNORM,
                        .usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT,
                    });
                builder.write(data.output, AccessDeclaration{
                                               .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           });
                board->add<FinalCompositePassData>(data);
            },

            [](const FinalCompositePassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderingDevice *device = RenderingDevice::get();
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                const FrameGraphTexture &texture = pass_resource.get<FrameGraphTexture>(data.output);
                device->begin_debug_utils_label(command_buffer, "FinalCompositePass", nullptr);

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_render_pass({
                                                      AttachmentInfo{
                                                          .texture = texture.id,
                                                          .load_op = LOAD_OP_CLEAR,
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

                ScopedGpuProfiling(command_buffer, "FinalCompositePass");
                command_buffer->end_render_pass();
                device->end_debug_utils_label(command_buffer);
            });
    }
} // namespace mirai