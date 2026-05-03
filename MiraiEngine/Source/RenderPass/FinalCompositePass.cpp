#include "FinalCompositePass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/AppSettings.hpp"
#include "Device/Window.hpp"

namespace mirai {

    FinalCompositePass::FinalCompositePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<FinalCompositePassData>(
            "FinalCompositePass",
            [board](FrameGraph::Builder &builder, FinalCompositePassData &data) {
                // @TODO we can skip the creation of this texture by copying directly to swapchain
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                data.output = builder.add_texture(K_SWAPCHAIN_TEXTURE_HANDLE, "Swapchain");

                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           });

#if 1
                const TAAResolvePassData &taa_resolve_data = board->get<TAAResolvePassData>();
                builder.read(taa_resolve_data.output, {
                                                          .access_flags = ACCESS_FLAG_SHADER_READ,
                                                          .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                          .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                      });
#else
                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                builder.read(forward_pass_data.color_texture, {
                                                                  .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                  .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                                  .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              });
#endif
                data.shader = std::make_shared<EffectMaterial>("FinalCompositeShader",
                                                               std::vector<std::string>{"SPIRV/fullscreen.vert.spv", "SPIRV/final-composite.frag.spv"},
                                                               PipelineState{
                                                                   .cull_mode = CULL_MODE_NONE,
                                                                   .depth_test = false,
                                                               },
                                                               PipelineAttachmentInfo{
                                                                   .color_attachments_format = {FORMAT_B8G8R8A8_UNORM},
                                                               });
                ASSERT(data.shader != nullptr);

                board->add<FinalCompositePassData>(data);
            },

            [](const FinalCompositePassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;

                auto resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                Renderer *renderer = Renderer::get();
                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                TextureID input_color_texture = TextureID{K_INVALID_RESOURCE_HANDLE};
#if 1
                const TAAResolvePassData &taa_pass_data = board->get<TAAResolvePassData>();
                input_color_texture = pass_resource.get<FrameGraphTexture>(taa_pass_data.output).id;
#else
                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                input_color_texture = pass_resource.get<FrameGraphTexture>(forward_pass_data.color_texture).id;
#endif
                ASSERT(input_color_texture.is_valid());

                const RenderDebugData &debug_data = board->get<RenderDebugData>();

                struct PushData {
                    float width;
                    float height;
                    float enable_gamma_correction;
                    float exposure;
                    uint32_t input_color_texture;
                    uint32_t _padding[3];
                } push_data;

                push_data.width = cast_float(width);
                push_data.height = cast_float(height);
                push_data.enable_gamma_correction = cast_float(debug_data.enable_gamma_correction);
                push_data.exposure = debug_data.exposure;
                push_data.input_color_texture = input_color_texture.id;

                command_buffer->begin_gpu_debug_label("FinalCompositePass");
                ScopedGpuProfiling(command_buffer, "FinalCompositePass");

                uint32_t swapchain_width, swapchain_height;
                Window::get()->get_size(&swapchain_width, &swapchain_height);
                command_buffer->begin_render_pass({AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                      .load_op = LOAD_OP_CLEAR,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                                  }},
                                                  {}, swapchain_width, swapchain_height);
                command_buffer->set_viewport({
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = cast_float(swapchain_width),
                    .height = cast_float(swapchain_height),
                    .min_depth = 0.0f,
                    .max_depth = 1.0f,
                });
                command_buffer->set_scissor(0, 0, swapchain_width, swapchain_height);

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_data, cast_u32(sizeof(push_data)));

                command_buffer->draw(3, 1, 0, 0);

                command_buffer->end_render_pass();

                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai