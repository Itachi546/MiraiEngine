#include "ForwardPass.hpp"

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/ShadowSystem.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Graphics/LineRenderer.hpp"
#include "Engine/Profiler.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/Camera.hpp"
namespace mirai {
    struct PushConstants {
        float split_percentage;
        int debug_param_index;
        float pcf_radius;
        float pcf_sample_count;

        float ibl_intensity;
        uint32_t num_lights;
        float light_culling;
        uint32_t tile_size;

        float mip_bias;
        uint32_t ssao_texture;
        uint32_t shadow_texture;
        uint32_t padding;
    };

    ForwardPass::ForwardPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<ForwardPassData>(
            "ForwardPass",
            [board](FrameGraph::Builder &builder, ForwardPassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                data.color_texture = builder.create_texture("FinalTexture", {
                                                                                .create_flags = 0,
                                                                                .width = width,
                                                                                .height = height,
                                                                                .depth = 1,
                                                                                .mip_levels = 1,
                                                                                .array_layers = 1,
                                                                                .texture_type = TEXTURE_TYPE_2D,
                                                                                .format = FORMAT_R16G16B16A16_SFLOAT,
                                                                                .usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT,
                                                                            });
                builder.write(data.color_texture, {
                                                      .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                                      .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                      .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                  });

                data.velocity_buffer = builder.create_texture("VelocityBuffer", {
                                                                                    .create_flags = 0,
                                                                                    .width = width,
                                                                                    .height = height,
                                                                                    .depth = 1,
                                                                                    .mip_levels = 1,
                                                                                    .array_layers = 1,
                                                                                    .texture_type = TEXTURE_TYPE_2D,
                                                                                    .format = FORMAT_R16G16_SFLOAT,
                                                                                    .usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                                                                                });
                builder.write(data.velocity_buffer, {
                                                        .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                                        .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                                        .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                                    });

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_READ | ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_WRITE,
                                                            .stage_mask = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                                            .layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                        });

                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                builder.read(ssao_pass_data.output, {
                                                        .access_flags = ACCESS_FLAG_SHADER_READ,
                                                        .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                        .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                    });

                const CascadedShadowPassData &csm_data = board->get<CascadedShadowPassData>();
                builder.read(csm_data.output, {
                                                  .access_flags = ACCESS_FLAG_SHADER_READ,
                                                  .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                  .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                              });

                const TiledLightCullPassData &light_cull_data = board->get<TiledLightCullPassData>();
                builder.read(light_cull_data.light_list_buffer, {
                                                                    .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                    .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                                });

                data.registry = ShaderRegistryMap::get()->get_registry(PASS_MODE_FORWARD);

                data.skybox_shader = std::make_shared<EffectMaterial>("OverlaySkyboxShader",
                                                                      std::vector<std::string>{"SPIRV/fullscreen.vert.spv", "SPIRV/skybox.frag.spv"},
                                                                      PipelineState{
                                                                          .cull_mode = CULL_MODE_NONE,
                                                                          .depth_test = true,
                                                                      },
                                                                      PipelineAttachmentInfo{
                                                                          .color_attachments_format = {FORMAT_R16G16B16A16_SFLOAT, FORMAT_R16G16_SFLOAT},
                                                                          .has_depth_attachment = true,
                                                                          .depth_attachment_format = FORMAT_D32_SFLOAT,
                                                                      });

                board->add<ForwardPassData>(data);
            },

            [](const ForwardPassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedGpuProfiling(command_buffer, "ForwardPass");
                command_buffer->begin_gpu_debug_label("ForwardPass");

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                Scene *scene = renderer->get_scene();
                EnvironmentMap *env_map = scene->get_environment_map();

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                const CascadedShadowPassData &csm_data = board->get<CascadedShadowPassData>();
                const TiledLightCullPassData &light_cull_data = board->get<TiledLightCullPassData>();

                DescriptorOffset cubemap_binding = renderer->get_or_create_descriptor(env_map->get_cubemap(), DescriptorType::SampledImage);
                DescriptorOffset light_list_binding = renderer->get_or_create_descriptor(pass_resource.get<FrameGraphBuffer>(light_cull_data.light_list_buffer).id, DescriptorType::StorageBuffer);

                ShadowSystem *shadow_system = ShadowSystem::get();
                const RenderDebugData &debug_data = board->get<RenderDebugData>();

                PushConstants push_constant_data = {
                    debug_data.split_percentage,
                    debug_data.debug_param_index,
                    shadow_system->dir_light_params.pcf_radius,
                    shadow_system->dir_light_params.pcf_sample_count,
                    AppSettings::ibl_contribution,
                    renderer->total_visible_lights,
                    cast_float(debug_data.light_culling),
                    AppSettings::K_LIGHT_TILE_SIZE,
                    AppSettings::enable_taa ? debug_data.mip_lod_bias : 0.0f,
                    pass_resource.get<FrameGraphTexture>(ssao_pass_data.output).id.id,
                    pass_resource.get<FrameGraphTexture>(csm_data.output).id.id,
                };

                PushData push_data = {
                    .data = &push_constant_data,
                    .offset = 0,
                    .size = cast_u32(sizeof(push_constant_data)),
                };

                command_buffer->begin_render_pass({AttachmentInfo{
                                                       .texture = pass_resource.get<FrameGraphTexture>(data.color_texture).id,
                                                       .load_op = LOAD_OP_CLEAR,
                                                       .store_op = STORE_OP_STORE,
                                                       .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                                   },
                                                   AttachmentInfo{
                                                       .texture = pass_resource.get<FrameGraphTexture>(data.velocity_buffer).id,
                                                       .load_op = LOAD_OP_CLEAR,
                                                       .store_op = STORE_OP_STORE,
                                                       .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                                   }},
                                                  AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id,
                                                      .load_op = LOAD_OP_LOAD,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {1.0f, 0.0f, 0.0f, 0.0f},
                                                  },
                                                  width, height);

                command_buffer->set_viewport({
                    .x = 0.0f,
                    .y = 0.0f,
                    .width = cast_float(width),
                    .height = cast_float(height),
                    .min_depth = 0.0f,
                    .max_depth = 1.0f,
                });
                command_buffer->set_scissor(0, 0, width, height);

                std::vector<DescriptorOffset> descriptors = {
                    renderer->per_frame_data_descriptor,
                    renderer->global_geometry_descriptor,
                    renderer->transform_descriptor,
                    0,
                    renderer->material_descriptor,
                    renderer->cascade_data_descriptor,
                    renderer->per_frame_light_descriptor,
                    light_list_binding,
                };

                auto draw_batch = [&](const std::vector<RenderBatch> &batches, RenderBatchType batch_type) {
                    for (const auto &batch : batches) {
                        if (batch.batch_type != batch_type)
                            continue;
                        Shader *shader = is_custom_sort_key(batch.sort_key)
                                             ? batch.custom_shader
                                             : data.registry->find(batch.sort_key);
                        ASSERT(shader != nullptr);
                        DrawBatch(command_buffer, batch, {
                                                             .shader = shader,
                                                             .descriptor_infos = descriptors,
                                                             .push_data = &push_data,
                                                             .draw_data_descriptor_index = 3,
                                                         });
                    }
                };

                draw_batch(renderer->main_render_batches, RENDERBATCH_TYPE_OPAQUE);
                draw_batch(renderer->main_render_batches, RENDERBATCH_TYPE_ALPHA_MASK);

                {
                    Camera *camera = scene->get_camera();
                    // Draw Skybox
                    glm::mat4 skybox_push_data[] = {
                        camera->get_inv_projection_transform(),
                        camera->get_inv_view_transform(),
                    };

                    data.skybox_shader->bind(command_buffer);
                    command_buffer->set_push_data(0, skybox_push_data, cast_u32(sizeof(skybox_push_data)));
                    command_buffer->set_push_data(cast_u32(sizeof(skybox_push_data)), &cubemap_binding, cast_u32(sizeof(uint32_t)));
                    command_buffer->draw(3, 1, 0, 0);

                    // DebugDraw line
                    LineRenderer::get()->render(command_buffer, camera->get_view_projection_transform());
                }

                draw_batch(renderer->main_render_batches, RENDERBATCH_TYPE_TRANSPARENT);

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai