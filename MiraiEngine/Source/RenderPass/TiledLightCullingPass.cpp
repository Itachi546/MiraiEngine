#include "TiledLightCullingPass.hpp"
#include "Engine/Profiler.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/LineRenderer.hpp"

namespace mirai {
    struct TiledLightCullRenderData {
        DescriptorOffset descriptors[3];
    };

    glm::uvec2 get_light_tile_count(uint32_t width, uint32_t height) {
        return glm::uvec2{
            ((width + AppSettings::K_LIGHT_TILE_SIZE - 1) / AppSettings::K_LIGHT_TILE_SIZE),
            ((height + AppSettings::K_LIGHT_TILE_SIZE - 1) / AppSettings::K_LIGHT_TILE_SIZE),
        };
    }

    TiledLightCullingPass::TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<TiledLightCullPassData>(
            "TiledLightCullingPass",
            [frame_graph, board](FrameGraph::FrameGraphBuilder &builder, TiledLightCullPassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                glm::uvec2 tile_count = get_light_tile_count(width, height);
                uint32_t total_tiles = tile_count.x * tile_count.y;
                // We allocate twice the required size, one for opaque and one for transparent
                // We also allocate single uint for each tile to allocate the light count in each tile
                uint32_t light_list_buffer_size = cast_u32((total_tiles * (AppSettings::K_MAX_LIGHT_PER_TILE + 1)) * 2 * sizeof(uint32_t));

                data.light_list_buffer = builder.create_buffer("LightListBuffer", {
                                                                                      .size = light_list_buffer_size,
                                                                                      .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                                                      .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                                                                  });

                builder.write(data.light_list_buffer, {
                                                          .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                          .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                      });

                const DepthPrePassData &depth_prepass = board->get<DepthPrePassData>();
                builder.read(depth_prepass.output, {
                                                       .access_flags = ACCESS_FLAG_SHADER_READ,
                                                       .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                       .layout = IMAGE_LAYOUT_GENERAL,
                                                   });

                data.depth_texture = depth_prepass.output;
                const TextureDescription &depth_texture_desc = frame_graph->get_texture_description(depth_prepass.output);
                data.depth_texture_width = depth_texture_desc.width;
                data.depth_texture_height = depth_texture_desc.height;
                data.shader = std::make_shared<ComputeShader>("TileLightCull", "SPIRV/tile-light-cull.comp.spv");

                board->add<TiledLightCullPassData>(data);
            },

            [](const TiledLightCullPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                const auto &pass_resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(pass_resource_states);

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                TiledLightCullRenderData *render_data = nullptr;
                if (!board->has<TiledLightCullRenderData>()) {
                    DescriptorInfo descriptor_infos[] = {
                        {
                            .type = DescriptorType::SampledImage,
                            .resource = pass_resource.get<FrameGraphTexture>(data.depth_texture).id,
                            .image_info = {0, ~0u, 0, ~0u},
                        },
                        {
                            .type = DescriptorType::StorageBuffer,
                            .resource = pass_resource.get<FrameGraphBuffer>(data.light_list_buffer).id,
                            .buffer_info = {0, ~0u},
                        },
                    };

                    DescriptorOffset descriptors = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));

                    render_data = &board->add<TiledLightCullRenderData>(TiledLightCullRenderData{
                        .descriptors = {
                            descriptors,
                            descriptors + 1,
                            0,
                        },
                    });
                } else {
                    render_data = &board->get<TiledLightCullRenderData>();
                }
                render_data->descriptors[2] = renderer->per_frame_light_descriptor;
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                glm::uvec2 tile_count = get_light_tile_count(width, height);

                Camera *camera = renderer->get_scene()->get_camera();
                struct PushData {
                    glm::mat4 invP;
                    glm::mat4 V;
                    uint32_t light_count;
                    uint32_t tile_count_x;
                    uint32_t tile_count_y;
                    uint32_t depth_texture_width;

                    uint32_t depth_texture_height;
                    uint32_t tile_size;
                    uint32_t _padding[2];
                } push_data;
                push_data.invP = camera->get_inv_projection_transform();
                push_data.V = camera->get_view_transform();
                push_data.light_count = renderer->total_visible_lights;
                push_data.tile_count_x = tile_count.x;
                push_data.tile_count_y = tile_count.y;
                push_data.depth_texture_width = data.depth_texture_width;
                push_data.depth_texture_height = data.depth_texture_height;
                push_data.tile_size = AppSettings::K_LIGHT_TILE_SIZE;
                push_data._padding[0] = push_data._padding[1] = 0;

                uint32_t push_data_size = cast_u32(sizeof(PushData));

                ScopedGpuProfiling(command_buffer, "LightListGeneration");
                command_buffer->begin_gpu_debug_label("LightCullPass");

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, render_data->descriptors, cast_u32(sizeof(render_data->descriptors)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, AppSettings::K_LIGHT_TILE_SIZE);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, AppSettings::K_LIGHT_TILE_SIZE);
                command_buffer->dispatch(local_size_x, local_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai