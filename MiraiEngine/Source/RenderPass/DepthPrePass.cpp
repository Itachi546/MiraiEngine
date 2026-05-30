#include "DepthPrePass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Scene/Camera.hpp"
#include "RenderPassData.hpp"
#include "Engine/AppSettings.hpp"
#include "Common/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    DepthPrePass::DepthPrePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<DepthPrePassData>(
            "DepthPrePass",
            [board](FrameGraph::Builder &builder, DepthPrePassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                data.output = builder.create_texture("DepthTexture", {
                                                                         .create_flags = 0,
                                                                         .width = width,
                                                                         .height = height,
                                                                         .depth = 1,
                                                                         .mip_levels = 1,
                                                                         .array_layers = 1,
                                                                         .texture_type = TEXTURE_TYPE_2D,
                                                                         .format = FORMAT_D32_SFLOAT,
                                                                         .usage_flags = TEXTURE_USAGE_DEPTH_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                     });
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                               .layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                           });

                board->add<DepthPrePassData>(data);
            },
            [](const DepthPrePassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedGpuProfiling(command_buffer, "DepthPrePass");
                ScopedCpuProfiling("DepthPrepass[Render]");

                command_buffer->begin_gpu_debug_label("DepthPrePass");

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_render_pass({},
                                                  AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                      .load_op = LOAD_OP_CLEAR,
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

                /*
                    Batch is changed by two things only
                    1. Shader
                    2. Buffer
                */
                ShaderRegistry *registry = ShaderRegistry::get();
                std::vector<DescriptorOffset> descriptor_infos = {renderer->per_frame_data_descriptor, renderer->transform_descriptor, 0, 0};
                const auto draw_batch = [&](const std::vector<RenderBatch> &render_batches, AlphaMode alpha_mode) {
                    for (const auto &batch : render_batches) {
                        if (batch.get_alpha_mode() != alpha_mode)
                            continue;

                        Shader *shader = registry->find(batch.get_pso_key());
                        ASSERT(shader != nullptr);
                        if (shader == nullptr)
                            continue;

                        // @TODO temp
                        descriptor_infos[2] = renderer->get_or_create_descriptor(batch.get_geometry_buffer(), DescriptorType::StorageBuffer);
                        descriptor_infos[3] = batch.draw_data_descriptor;

                        DrawBatch(command_buffer, batch, {
                                                             .shader = shader,
                                                             .descriptor_infos = descriptor_infos,
                                                             .push_data = nullptr,
                                                         });
                    }
                };

                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                std::vector<RenderBatch> render_batches;
                DrawBatchGenerator::BuildBatches(
                    renderer->get_scene(),
                    BatchBuildParams{
                        .frustum = &camera->get_frustum_planes(),
                        .pass = PASS_MODE_DEPTH_PREPASS,
                    },
                    render_batches);

                if (render_batches.size() > 0) {
                    renderer->upload_batch_data(render_batches);

                    // Draw Opaque object
                    draw_batch(render_batches, ALPHA_MODE_OPAQUE);

                    descriptor_infos.push_back(renderer->material_descriptor);
                    draw_batch(render_batches, ALPHA_MODE_MASK);
                }

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai