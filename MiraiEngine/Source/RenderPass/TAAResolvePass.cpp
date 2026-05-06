#include "TAAResolvePass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Engine/AppSettings.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/TextureCache.hpp"
namespace mirai {

    uint32_t current_taa_texture = 0;
    TAAResolvePass::TAAResolvePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<TAAResolvePassData>(
            "TAAResolvePass",
            [board](FrameGraph::Builder &builder, TAAResolvePassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                TextureDescription texture_desc = {
                    .create_flags = 0,
                    .width = width,
                    .height = height,
                    .depth = 1,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .texture_type = TEXTURE_TYPE_2D,
                    .format = FORMAT_R16G16B16A16_SFLOAT,
                    .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
                };

                RenderingDevice *device = RenderingDevice::get();
                data.history_textures[0] = device->create_texture(&texture_desc, "taa_history_0");
                data.history_textures[1] = device->create_texture(&texture_desc, "taa_history_1");
                Renderer::get()->add_bindless_texture(data.history_textures[0]);
                Renderer::get()->add_bindless_texture(data.history_textures[1]);

                // We let cache manage the lifecycle of the texture
                TextureCache::get()->add_texture("TAAHistory0", data.history_textures[0]);
                TextureCache::get()->add_texture("TAAHistory1", data.history_textures[1]);

                data.output = builder.add_texture(data.history_textures[0], "TAAOutputTexture");
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });

                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                builder.read(forward_pass_data.color_texture, {
                                                                  .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                  .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                  .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              });
                builder.read(forward_pass_data.velocity_buffer, {
                                                                    .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                    .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                    .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                                });
                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_SHADER_READ,
                                                            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                            .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                        });

                data.shader = std::make_shared<ComputeShader>("TAAResolveShader", "SPIRV/taa-resolve.comp.spv");
                board->add<TAAResolvePassData>(data);

                board->add<TAAOptions>(TAAOptions{
                    .should_reset = true,
                    .should_sample_motion_vector = true,
                });
            },
            [](const TAAResolvePassData &data, const FrameGraphPassResource &pass_resources, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                current_taa_texture = 1 - current_taa_texture;

                ScopedGpuProfiling(command_buffer, "TAAResolve");
                command_buffer->begin_gpu_debug_label("TAAResolvePass");

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                TAAOptions &options = board->get<TAAOptions>();

                if (options.should_reset) {
                    // Copy the deferred texture output to TAA history if it is first frame or the texture has been reset
                    TextureID color_output_texture = pass_resources.get<FrameGraphTexture>(forward_pass_data.color_texture).id;
                    TextureBarrierInfo barrier_infos[] = {
                        {
                            .texture_id = color_output_texture,
                            .stage_mask = PIPELINE_STAGE_TRANSFER_BIT,
                            .access_mask = ACCESS_FLAG_TRANSFER_READ,
                            .layout = IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        },
                        {
                            .texture_id = data.history_textures[current_taa_texture],
                            .stage_mask = PIPELINE_STAGE_TRANSFER_BIT,
                            .access_mask = ACCESS_FLAG_TRANSFER_WRITE,
                            .layout = IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        },
                    };

                    command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

                    command_buffer->copy_texture(data.history_textures[current_taa_texture], color_output_texture, width, height);

                    options.should_reset = false;
                } else {
                    const auto &resource_states = pass_resources.get_resource_access_states();
                    command_buffer->prepare_resources(resource_states);

                    const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                    DescriptorOffset descriptors[] = {
                        renderer->get_or_create_descriptor(data.history_textures[current_taa_texture], DescriptorType::StorageImage),
                    };

                    uint32_t flags = cast_u32(AppSettings::enable_taa);
                    flags = (flags | cast_u32(options.should_sample_motion_vector) << 1);

                    uint32_t push_constant_data[] = {
                        cast_u32(width),
                        cast_u32(height),
                        flags,
                        pass_resources.get<FrameGraphTexture>(forward_pass_data.color_texture).id.id,
                        data.history_textures[1 - current_taa_texture].id,
                        pass_resources.get<FrameGraphTexture>(depth_prepass_data.output).id.id,
                        pass_resources.get<FrameGraphTexture>(forward_pass_data.velocity_buffer).id.id,
                        0,
                    };

                    TextureBarrierInfo barrier_infos[] = {
                        {
                            .texture_id = data.history_textures[1 - current_taa_texture],
                            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            .access_mask = ACCESS_FLAG_SHADER_READ,
                            .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                        },
                    };

                    command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

                    data.shader->bind(command_buffer);

                    uint32_t push_data_size = cast_u32(sizeof(push_constant_data));
                    command_buffer->set_push_data(0, push_constant_data, push_data_size);
                    command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                    uint32_t work_size_x = rendering_utils::get_workgroup_size(width, 32);
                    uint32_t work_size_y = rendering_utils::get_workgroup_size(height, 32);
                    command_buffer->dispatch(work_size_x, work_size_y, 1);
                }
                command_buffer->end_gpu_debug_label();

                // @Note hack, update the current output texture
                renderer->get_frame_graph()->get<FrameGraphTexture>(data.output).id = data.history_textures[current_taa_texture];
            });
    }
} // namespace mirai
