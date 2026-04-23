#include "DepthPrePass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"
#include "Engine/AppSettings.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    DepthPrePass::DepthPrePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<DepthPrePassData>(
            "DepthPrePass",
            [board](FrameGraph::FrameGraphBuilder &builder, DepthPrePassData &data) {
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
                data.registry = ShaderRegistryMap::get()->get_registry(PASS_MODE_DEPTH_PREPASS);
                ASSERT(data.registry != nullptr);
                board->add<DepthPrePassData>(data);
            },
            [](const DepthPrePassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedGpuProfiling(command_buffer, "DepthPrePass");
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

                // Draw Opaque object
                std::vector<DescriptorOffset> descriptor_infos = {renderer->per_frame_data_descriptor, renderer->transform_descriptor, renderer->global_geometry_descriptor, 0};
                for (const auto &batch : renderer->main_render_batches) {
                    if (batch.batch_type != RENDERBATCH_TYPE_OPAQUE)
                        continue;

                    Shader *shader = data.registry->find(batch.sort_key);
                    ASSERT(shader != nullptr);

                    DrawBatch(command_buffer, batch, {
                                                         .shader = shader,
                                                         .descriptor_infos = descriptor_infos,
                                                         .push_data = nullptr,
                                                         // Used to override descriptor info at given index
                                                         .draw_data_descriptor_index = 3,
                                                     });
                }

                descriptor_infos.push_back(renderer->material_descriptor);
                for (const auto &batch : renderer->main_render_batches) {
                    if (batch.batch_type != RENDERBATCH_TYPE_ALPHA_MASK)
                        continue;
                    Shader *shader = data.registry->find(batch.sort_key);
                    ASSERT(shader != nullptr);
                    DrawBatch(command_buffer, batch, {
                                                         .shader = shader,
                                                         .descriptor_infos = descriptor_infos,
                                                         .push_data = nullptr,
                                                         .draw_data_descriptor_index = 3,
                                                     });
                }

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai