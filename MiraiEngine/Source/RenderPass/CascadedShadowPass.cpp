#include "CascadedShadowPass.hpp"
#include "Math/Frustum.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"
#include "Scene/RenderBatch.hpp"
#include "Scene/ShadowSystem.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
namespace mirai {
    CascadedShadowPass::CascadedShadowPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<CascadedShadowPassData>(
            "CascadedShadowPass",
            [board](FrameGraph::Builder &builder, CascadedShadowPassData &data) {
                ShadowSystem *shadow_system = ShadowSystem::get();
                const DirectionLightShadowParams &params = shadow_system->dir_light_params;

                data.output = builder.create_texture("CascadeDepthTexture", {
                                                                                .create_flags = 0,
                                                                                .width = params.atlas_size,
                                                                                .height = params.atlas_size,
                                                                                .depth = 1,
                                                                                .mip_levels = 1,
                                                                                .array_layers = 1,
                                                                                .texture_type = TEXTURE_TYPE_2D,
                                                                                .format = FORMAT_D32_SFLOAT,
                                                                                .usage_flags = TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                                                                            });

                const SkinningComputePassData &skinning_data = board->get<SkinningComputePassData>();
                builder.read(skinning_data.output_buffer, {
                                                              .access_flags = ACCESS_FLAG_SHADER_READ,
                                                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                          });

                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                               .layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                           });
                data.registry = ShaderRegistryMap::get()->get_registry(PASS_MODE_DIRLIGHT_SHADOW);
                ASSERT(data.registry != nullptr);

                board->add<CascadedShadowPassData>(data);
            },
            [](const CascadedShadowPassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedGpuProfiling(command_buffer, "CascadedShadowPass");
                ScopedCpuProfiling("CSM Render");
                command_buffer->begin_gpu_debug_label("CascadedShadowPass");

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                ShadowSystem *shadow_system = ShadowSystem::get();
                const DirectionLightShadowParams &shadow_params = shadow_system->dir_light_params;

                command_buffer->begin_render_pass({},
                                                  AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                      .load_op = LOAD_OP_CLEAR,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {1.0f, 0.0f, 0.0f, 0.0f},
                                                  },
                                                  shadow_params.atlas_size, shadow_params.atlas_size);

                if (shadow_params.enabled) {
                    uint32_t push_constant_data[] = {0, 0, 0, 0};
                    PushData push_constants = {
                        .data = &push_constant_data,
                        .offset = 0,
                        .size = sizeof(uint32_t) * 4,
                    };

                    FrustumPlanes frustum_planes;
                    std::vector<RenderBatch> render_batches;
                    const DirectionalLightCascadeInfo &cascade_info = shadow_system->cascade_info;

                    std::vector<DescriptorOffset> descriptors = {
                        renderer->cascade_data_descriptor,
                        renderer->global_geometry_descriptor,
                        0,
                        renderer->transform_descriptor,
                    };

                    uint32_t current_frame = RenderingDevice::get()->get_current_frame();

                    for (uint32_t i = 0; i < NUM_DIRLIGHT_CASCADE; ++i) {
                        std::string split = "Split" + std::to_string(i);
                        ScopedGpuProfiling(command_buffer, split.c_str());

                        // This create both frustum plane and points, we only need plane
                        frustum_planes.create_from_matrix(cascade_info.VP[i]);

                        // Shadow pass: front-cull opaque geometry to prevent shadow acne.
                        // Alpha-mask objects get CULL_NONE applied automatically in BuildBatches.
                        static const MaterialState shadow_state{.cull_mode = CULL_MODE_FRONT};
                        DrawBatchGenerator::BuildBatches(
                            renderer->get_scene(),
                            BatchBuildParams{
                                .filter_flags = BATCH_FILTER_FLAG_OPAQUE | BATCH_FILTER_FLAG_ALPHA_MASK,
                                .frustum = &frustum_planes,
                                .pass_state_override = &shadow_state,
                                .shadow_pass = true,
                            },
                            render_batches);

                        if (render_batches.size() == 0)
                            continue;

                        renderer->upload_batch_data(render_batches, current_frame);

                        // Update push constants
                        push_constant_data[0] = i;

                        command_buffer->begin_gpu_debug_label(split.c_str());

                        uint32_t y = (i / 2) * shadow_params.split_size;
                        uint32_t x = (i % 2) * shadow_params.split_size;
                        uint32_t width = shadow_params.split_size;
                        uint32_t height = shadow_params.split_size;

                        command_buffer->set_viewport({cast_float(x), cast_float(y), cast_float(width), cast_float(height), 0.0f, 1.0f});

                        command_buffer->set_scissor(x, y, width, height);

                        // Draw Opaque batch
                        for (const auto &batch : render_batches) {
                            if (batch.batch_type == RENDERBATCH_TYPE_OPAQUE && batch.meshes.size() > 0) {
                                Shader *shader = data.registry->find(batch.sort_key);
                                ASSERT(shader != nullptr);
                                DrawBatch(command_buffer, batch, {
                                                                     .shader = shader,
                                                                     .descriptor_infos = descriptors,
                                                                     .push_data = &push_constants,
                                                                     .draw_data_descriptor_index = 2,
                                                                 });
                            }
                        }

                        // Draw Alpha mask batch
                        descriptors.push_back(renderer->material_descriptor);
                        for (const auto &batch : render_batches) {
                            if (batch.batch_type == RENDERBATCH_TYPE_ALPHA_MASK && batch.meshes.size() > 0) {
                                Shader *shader = data.registry->find(batch.sort_key);
                                ASSERT(shader != nullptr);
                                DrawBatch(command_buffer, batch, {
                                                                     .shader = shader,
                                                                     .descriptor_infos = descriptors,
                                                                     .push_data = &push_constants,
                                                                     .draw_data_descriptor_index = 2,
                                                                 });
                            }
                        }

                        command_buffer->end_gpu_debug_label();

                        render_batches.clear();
                    }
                }
                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai