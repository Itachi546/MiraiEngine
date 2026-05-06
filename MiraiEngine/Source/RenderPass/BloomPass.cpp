#include "BloomPass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    constexpr const uint32_t K_MAX_BLOOM_MIP_LEVELS = 6;

    BloomPass::BloomPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<BloomPassData>(
            "BloomPass",
            [board](FrameGraph::Builder &builder, BloomPassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                data.output = builder.create_texture(
                    "BloomTexture",
                    {
                        .create_flags = 0,
                        .width = width,
                        .height = height,
                        .depth = 1,
                        .mip_levels = K_MAX_BLOOM_MIP_LEVELS,
                        .array_layers = 1,
                        .texture_type = TEXTURE_TYPE_2D,
                        .format = FORMAT_B10G11R11_UFLOAT_PACK32,
                        .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                    });
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_READ,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });

                const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                builder.read(forward_pass_data.color_texture, {
                                                                  .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                  .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                  .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                              });

                data.downsample_shader = std::make_shared<ComputeShader>("BloomDownsample", "SPIRV/bloom-downsample.comp.spv");

                board->add<BloomPassData>(data);

                board->add<BloomOptions>(BloomOptions{
                    .threshold = 0.8f,
                });
            },
            [](const BloomPassData &data, const FrameGraphPassResource &pass_resources, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;
                Renderer *renderer = ctx->renderer;

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                ScopedGpuProfiling(command_buffer, "Bloom");
                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                const BloomOptions &bloom_options = board->get<BloomOptions>();
                TextureID bloom_texture = pass_resources.get<FrameGraphTexture>(data.output).id;

                struct PushData {
                    uint32_t input_texture_index;
                    uint32_t input_mip_level;
                    uint32_t width;
                    uint32_t height;
                } push_data;

                // Generate mip 0, bloom texture
                {
                    const auto &resource_states = pass_resources.get_resource_access_states();
                    command_buffer->prepare_resources(resource_states);

                    const ForwardPassData &forward_pass_data = board->get<ForwardPassData>();
                    push_data.input_texture_index = pass_resources.get<FrameGraphTexture>(forward_pass_data.color_texture).id.id;
                    push_data.input_mip_level = 0;
                    push_data.width = width;
                    push_data.height = height;

                    DescriptorOffset descriptors[] = {
                        renderer->get_or_create_descriptor(bloom_texture, DescriptorType::StorageImage),
                    };

                    data.downsample_shader->bind(command_buffer);

                    uint32_t push_data_size = cast_u32(sizeof(push_data));
                    command_buffer->set_push_data(0, &push_data, push_data_size);
                    command_buffer->set_push_data(push_data_size, &descriptors, cast_u32(sizeof(descriptors)));

                    uint32_t work_size_x = rendering_utils::get_workgroup_size(push_data.width, 32);
                    uint32_t work_size_y = rendering_utils::get_workgroup_size(push_data.height, 32);

                    command_buffer->dispatch(work_size_x, work_size_y, 1);
                }

                // Downsample
                TextureMipBarrierInfo barriers[] = {
                    {
                        .texture_id = bloom_texture,
                        .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        .access_mask = ACCESS_FLAG_SHADER_READ,
                        .layout = IMAGE_LAYOUT_GENERAL,
                        .mip_level = 0,
                        .mip_count = 1,
                        .array_level = 0,
                        .array_count = ~0u,
                    },
                };
                {
                    uint32_t mip_width = width / 2;
                    uint32_t mip_height = height / 2;

                    data.downsample_shader->bind(command_buffer);

                    for (uint32_t i = 1; i < K_MAX_BLOOM_MIP_LEVELS; ++i) {
                        command_buffer->prepare_image_mip(barriers, cast_u32(std::size(barriers)));

                        DescriptorOffset descriptors[] = {
                            renderer->get_or_create_descriptor("bloom_storage_image" + std::to_string(i), DescriptorInfo{
                                                                                                              .type = DescriptorType::StorageImage,
                                                                                                              .resource = bloom_texture,
                                                                                                              .image_info = {i, 1, 0, ~0u},
                                                                                                          }),
                        };

                        push_data = {bloom_texture.id, i - 1, mip_width, mip_height};

                        uint32_t push_data_size = cast_u32(sizeof(push_data));
                        command_buffer->set_push_data(0, &push_data, push_data_size);
                        command_buffer->set_push_data(push_data_size, &descriptors, cast_u32(sizeof(descriptors)));

                        uint32_t work_size_x = rendering_utils::get_workgroup_size(mip_width, 32);
                        uint32_t work_size_y = rendering_utils::get_workgroup_size(mip_height, 32);

                        command_buffer->dispatch(work_size_x, work_size_y, 1);

                        barriers[0].mip_level = i;

                        mip_width /= 2;
                        mip_height /= 2;
                    }
                }
            });
    }
} // namespace mirai