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

    struct TiledLightPassBindings {
        DescriptorOffset frustum_binding;
    };

    const uint32_t TILE_SIZE = 16;
    TiledLightCullingPass::TiledLightCullingPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
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

                builder.read(data.buffer, {
                                              .access_flags = ACCESS_FLAG_SHADER_READ,
                                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                          });

                data.shader = std::make_shared<ComputeShader>("LightTileFrustumGen", "SPIRV/tile-frustum-gen.comp.spv");

                // @TODO temp
                board->add<TiledLightCullingFrustumPassData>(data);
                builder.set_side_effect();
            },

            [](const TiledLightCullingFrustumPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                const auto &pass_resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(pass_resource_states);

                TiledLightPassBindings *bindings = nullptr;
                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                if (!board->has<TiledLightPassBindings>()) {
                    DescriptorInfo descriptor_info = {
                        .type = DescriptorType::StorageBuffer,
                        .resource = pass_resource.get<FrameGraphBuffer>(data.buffer).id,
                        .buffer_info = {
                            0,
                            ~0u,
                        }};
                    DescriptorOffset descriptor = renderer->resource_heap.push_descriptors(RenderingDevice::get(), &descriptor_info, 1);
                    bindings = &board->add<TiledLightPassBindings>(TiledLightPassBindings{
                        .frustum_binding = descriptor,
                    });
                } else {
                    bindings = &board->get<TiledLightPassBindings>();
                }

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                struct PushConstantData {
                    glm::mat4 invP;
                    glm::vec2 resolution;
                    float tile_size;
                    float _padding;
                } push_data;
                push_data.invP = renderer->get_scene()->get_camera()->get_inv_projection_transform();
                push_data.resolution = glm::vec2(cast_float(width), cast_float(height));
                push_data.tile_size = TILE_SIZE;
                push_data._padding = 0;

                ScopedGpuProfiling(command_buffer, "Tile Frustum Generation");
                command_buffer->begin_gpu_debug_label("Tile Frustum Generation");

                data.shader->bind(command_buffer);
                uint32_t push_constant_data_size = cast_u32(sizeof(PushConstantData));
                command_buffer->set_push_data(0, &push_data, push_constant_data_size);
                command_buffer->set_push_data(push_constant_data_size, &bindings->frustum_binding, cast_u32(sizeof(uint32_t)));

                uint32_t tile_count_x = ((width + TILE_SIZE - 1) / TILE_SIZE);
                uint32_t tile_count_y = ((height + TILE_SIZE - 1) / TILE_SIZE);

                uint32_t local_size_x = rendering_utils::get_workgroup_size(tile_count_x, 32);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(tile_count_y, 32);

                command_buffer->dispatch(local_size_x, local_size_y, 1);

                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai