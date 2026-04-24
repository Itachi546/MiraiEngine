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
            [board](FrameGraph::Builder &builder, FinalCompositePassData &data) {
                // @TODO we can skip the creation of this texture by copying directly to swapchain
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

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
                        .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                    });

                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });

                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                builder.read(forward_pass_data.output, {
                                                           .access_flags = ACCESS_FLAG_SHADER_READ,
                                                           .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                           .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                       });
                data.input = forward_pass_data.output;

                data.shader = std::make_shared<ComputeShader>("FinalCompositeShader", "SPIRV/final-composite.comp.spv");
                ASSERT(data.shader != nullptr);

                board->add<FinalCompositePassData>(data);
            },

            [](const FinalCompositePassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                Renderer *renderer = Renderer::get();
                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                DescriptorOffset bindings[] = {
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.input).id, DescriptorType::SampledImage),
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
                };

                const RenderDebugData &debug_data = board->get<RenderDebugData>();
                float push_data[] = {
                    cast_float(width),
                    cast_float(height),
                    cast_float(debug_data.enable_gamma_correction),
                    0.0f,
                };

                command_buffer->begin_gpu_debug_label("FinalCompositePass");
                ScopedGpuProfiling(command_buffer, "FinalCompositePass");

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, push_data, cast_u32(sizeof(push_data)));
                command_buffer->set_push_data(cast_u32(sizeof(push_data)), bindings, cast_u32(sizeof(bindings)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, 32);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, 32);
                command_buffer->dispatch(local_size_x, local_size_y, 1);

                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai