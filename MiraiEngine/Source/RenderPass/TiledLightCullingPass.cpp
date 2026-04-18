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
    struct TiledLightCullingFrustumPassData {
        FrameGraphResourceHandle buffer;
        std::shared_ptr<ComputeShader> shader;
    };
    struct TiledLightFrustumRenderData {
        bool regenerate_frustum;
        DescriptorOffset frustum_buffer_binding;
    };

    struct TiledLightCullRenderData {
        DescriptorOffset descriptors[4];
    };

    const uint32_t TILE_SIZE = 16;
    TiledLightCullingPass::TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        // Generate tile frustum
        frame_graph->add_callback_pass<TiledLightCullingFrustumPassData>(
            "TiledLightCullingFrustumPass",
            [board](FrameGraph::FrameGraphBuilder &builder, TiledLightCullingFrustumPassData &data) {
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                uint32_t tile_count = ((width + TILE_SIZE - 1) / TILE_SIZE) * ((height + TILE_SIZE - 1) / TILE_SIZE);
                uint32_t buffer_size = cast_u32(tile_count * sizeof(glm::vec4) * 4);
                data.buffer = builder.create_buffer("FrustumBuffer", {
                                                                         // 4 planes for each tile
                                                                         .size = buffer_size,
                                                                         .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                                         .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                                                     });

                builder.write(data.buffer, {
                                               .access_flags = ACCESS_FLAG_SHADER_READ,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                           });

                data.shader = std::make_shared<ComputeShader>("LightTileFrustumGen", "SPIRV/tile-frustum-gen.comp.spv");
                board->add<TiledLightCullingFrustumPassData>(data);
            },

            [](const TiledLightCullingFrustumPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                const auto &pass_resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(pass_resource_states);

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                TiledLightFrustumRenderData *render_data = nullptr;
                if (!board->has<TiledLightFrustumRenderData>()) {
                    DescriptorInfo descriptor_info = {
                        .type = DescriptorType::StorageBuffer,
                        .resource = pass_resource.get<FrameGraphBuffer>(data.buffer).id,
                        .buffer_info = {
                            0,
                            ~0u,
                        }};
                    DescriptorOffset descriptor = renderer->resource_heap.push_descriptors(RenderingDevice::get(), &descriptor_info, 1);
                    render_data = &board->add<TiledLightFrustumRenderData>(TiledLightFrustumRenderData{
                        .regenerate_frustum = false,
                        .frustum_buffer_binding = descriptor,
                    });
                } else {
                    render_data = &board->get<TiledLightFrustumRenderData>();
                }

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                uint32_t tile_count_x = ((width + TILE_SIZE - 1) / TILE_SIZE);
                uint32_t tile_count_y = ((height + TILE_SIZE - 1) / TILE_SIZE);

                const glm::mat4 &invP = renderer->get_scene()->get_camera()->get_inv_projection_transform();
                // Generate tile frustum in view space
                if (render_data->regenerate_frustum) {
                    struct PushConstantData {
                        glm::mat4 invP;
                        glm::vec2 resolution;
                        int tile_size;
                        int tile_count_x;
                    } push_data;

                    push_data.invP = invP;
                    push_data.resolution = glm::vec2(cast_float(width), cast_float(height));
                    push_data.tile_size = TILE_SIZE;
                    push_data.tile_count_x = tile_count_x;

                    ScopedGpuProfiling(command_buffer, "Tile Frustum Generation");
                    command_buffer->begin_gpu_debug_label("Tile Frustum Generation");

                    data.shader->bind(command_buffer);
                    uint32_t push_constant_data_size = cast_u32(sizeof(PushConstantData));
                    command_buffer->set_push_data(0, &push_data, push_constant_data_size);
                    command_buffer->set_push_data(push_constant_data_size, &render_data->frustum_buffer_binding, cast_u32(sizeof(uint32_t)));

                    uint32_t local_size_x = rendering_utils::get_workgroup_size(tile_count_x, TILE_SIZE);
                    uint32_t local_size_y = rendering_utils::get_workgroup_size(tile_count_y, TILE_SIZE);
                    command_buffer->dispatch(local_size_x, local_size_y, 1);

                    command_buffer->end_gpu_debug_label();
                }
            });

        // Cull lights and assign to tile
        frame_graph->add_callback_pass<TiledLightCullPassData>(
            "TiledLightCullingPass",
            [frame_graph, board](FrameGraph::FrameGraphBuilder &builder, TiledLightCullPassData &data) {
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                data.debug_texture = builder.create_texture(
                    "TiledDebugTexture",
                    {
                        .create_flags = 0,
                        .width = width,
                        .height = height,
                        .depth = 1,
                        .mip_levels = 1,
                        .array_layers = 1,
                        .texture_type = TEXTURE_TYPE_2D,
                        .format = FORMAT_B8G8R8A8_UNORM,
                        .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                    });

                builder.write(data.debug_texture, {
                                                      .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                      .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                      .layout = IMAGE_LAYOUT_GENERAL,
                                                  });

                const TiledLightCullingFrustumPassData &frustum_data = board->get<TiledLightCullingFrustumPassData>();
                builder.read(frustum_data.buffer, {
                                                      .access_flags = ACCESS_FLAG_SHADER_READ,
                                                      .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                  });
                data.frustum_buffer = frustum_data.buffer;

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
                            .type = DescriptorType::StorageImage,
                            .resource = pass_resource.get<FrameGraphTexture>(data.debug_texture).id,
                            .image_info = {0, ~0u, 0, ~0u},
                        },
                    };

                    DescriptorOffset descriptors = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));

                    const TiledLightFrustumRenderData &frustum_render_data = board->get<TiledLightFrustumRenderData>();
                    render_data = &board->add<TiledLightCullRenderData>(TiledLightCullRenderData{
                        .descriptors = {
                            descriptors,
                            frustum_render_data.frustum_buffer_binding,
                            0,
                            descriptors + 1,
                        },
                    });
                } else {
                    render_data = &board->get<TiledLightCullRenderData>();
                }
                render_data->descriptors[2] = renderer->per_frame_light_descriptor;
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                uint32_t tile_count_x = ((width + TILE_SIZE - 1) / TILE_SIZE);
                uint32_t tile_count_y = ((height + TILE_SIZE - 1) / TILE_SIZE);

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
                push_data.tile_count_x = tile_count_x;
                push_data.tile_count_y = tile_count_y;
                push_data.depth_texture_width = 1920;
                push_data.depth_texture_height = 1080;
                push_data.tile_size = TILE_SIZE;
                push_data._padding[0] = push_data._padding[1] = 0;

                uint32_t push_data_size = cast_u32(sizeof(PushData));

                ScopedGpuProfiling(command_buffer, "LightListGeneration");
                command_buffer->begin_gpu_debug_label("LightCullPass");

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, render_data->descriptors, cast_u32(sizeof(render_data->descriptors)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, TILE_SIZE);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, TILE_SIZE);
                command_buffer->dispatch(local_size_x, local_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai