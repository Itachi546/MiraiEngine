#include "ForwardPass.hpp"

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    struct ForwardPassBindings {
        DescriptorOffset ssao_binding;
        DescriptorOffset csm_binding;
    };

    ForwardPass::ForwardPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<ForwardPassData>(
            "ForwardPass",
            [board](FrameGraph::FrameGraphBuilder &builder, ForwardPassData &data) {
                RenderingDevice *device = RenderingDevice::get();
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                data.output = builder.create_texture("FinalTexture", {
                                                                         .create_flags = 0,
                                                                         .width = width,
                                                                         .height = height,
                                                                         .depth = 1,
                                                                         .mip_levels = 1,
                                                                         .array_layers = 1,
                                                                         .texture_type = TEXTURE_TYPE_2D,
                                                                         .format = FORMAT_B8G8R8A8_UNORM,
                                                                         .usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT,
                                                                     });
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                               .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                           });

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_DEPTH_STENCIL_ATTACHMENT_READ,
                                                            .stage_mask = PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                                                            .layout = IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                                                        });
                data.depth_texture = depth_prepass_data.output;

                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                builder.read(ssao_pass_data.output, {
                                                        .access_flags = ACCESS_FLAG_SHADER_READ,
                                                        .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                        .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                    });
                data.ssao_texture = ssao_pass_data.output;

                const CascadedShadowPassData &csm_data = board->get<CascadedShadowPassData>();
                builder.read(csm_data.output, {
                                                  .access_flags = ACCESS_FLAG_SHADER_READ,
                                                  .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                  .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                              });
                data.csm_texture = csm_data.output;

                data.registry = ShaderRegistryMap::get()->get_registry(PASS_MODE_FORWARD);
                board->add<ForwardPassData>(data);
            },

            [](const ForwardPassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedGpuProfiling(command_buffer, "ForwardPass");
                command_buffer->begin_gpu_debug_label("ForwardPass");

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                ForwardPassBindings *bindings = nullptr;
                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                if (!board->has<ForwardPassBindings>()) {
                    DescriptorInfo descriptor_infos[] = {
                        {
                            .type = DescriptorType::SampledImage,
                            .resource = pass_resource.get<FrameGraphTexture>(data.ssao_texture).id,
                            .image_info = {0, ~0u, 0, ~0u},
                        },
                        {
                            .type = DescriptorType::SampledImage,
                            .resource = pass_resource.get<FrameGraphTexture>(data.csm_texture).id,
                            .image_info = {0, ~0u, 0, ~0u},
                        },
                    };
                    DescriptorOffset descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
                    bindings = &board->add<ForwardPassBindings>(ForwardPassBindings{
                        .ssao_binding = descriptor_offset,
                        .csm_binding = descriptor_offset + 1,
                    });
                } else {
                    bindings = &board->get<ForwardPassBindings>();
                }
                ASSERT(bindings != nullptr);

                const RenderDebugData &debug_data = board->get<RenderDebugData>();
                float push_constants[] = {debug_data.split_percentage, cast_float(debug_data.debug_param_index), cast_float(debug_data.show_debug_cascade_color), 0.0f};
                PushData push_data = {
                    .data = push_constants,
                    .offset = 0,
                    .size = cast_u32(sizeof(push_constants)),
                };

                command_buffer->begin_render_pass({AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                      .load_op = LOAD_OP_CLEAR,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                                  }},
                                                  AttachmentInfo{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.depth_texture).id,
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
                    bindings->ssao_binding,
                    bindings->csm_binding,
                    renderer->cascade_data_descriptor,
                };

                const std::vector<RenderBatch> &opaque_batches = renderer->main_opaque_batches;
                for (const auto &batch : opaque_batches) {
                    Shader *shader = data.registry->find(batch.sort_key);
                    ASSERT(shader != nullptr);
                    DrawBatch(command_buffer, batch, {
                                                         .shader = shader,
                                                         .descriptor_infos = descriptors,
                                                         .push_data = &push_data,
                                                         .draw_data_descriptor_index = 3,
                                                     });
                }
                const std::vector<RenderBatch> &alpha_mask_batches = renderer->main_alpha_mask_batches;
                for (const auto &batch : alpha_mask_batches) {
                    Shader *shader = data.registry->find(batch.sort_key);
                    ASSERT(shader != nullptr);
                    DrawBatch(command_buffer, batch, {
                                                         .shader = shader,
                                                         .descriptor_infos = descriptors,
                                                         .push_data = &push_data,
                                                         .draw_data_descriptor_index = 3,
                                                     });
                }

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai

// #include "ForwardPass.hpp"
// #include "Scene/ShaderHashMap.hpp"
// #include "Scene/RenderBatch.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Engine/Profiler.hpp"
// #include "Graphics/Renderer.hpp"

// namespace mirai {
//     ForwardPass::ForwardPass() : FrameGraphRenderer("forward_pass") {
//     }

//     void ForwardPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         ASSERT(node != nullptr);
//         ScopedCpuProfiling("FrameGraph::ForwardPass");

//         ScopedGpuProfiling(command_buffer, "Forward Pass");

//         device->begin_debug_utils_label(command_buffer, "ForwardPass", nullptr);

//         command_buffer->begin_render_pass(node, frame_graph);

//         std::vector<RenderBatch> &render_batches = renderer->main_render_batches;

//         float push_constant_data[] = {(float)debug_texture, split_percentage * node->width, 0, 0};
//         PushConstant push_constant = {
//             .data = push_constant_data,
//             .offset = 0,
//             .size = sizeof(float) * 4,
//             .shader_stage = SHADER_STAGE_FRAGMENT,
//         };

//         auto draw_batch = [&](RenderBatchType render_batch_type, PipelineState &pipeline_state) {
//             for (auto &batch : render_batches) {
//                 if (batch.batch_type != render_batch_type)
//                     continue;
//                 pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
//                 pipeline_state.render_state.fields.pass_mode = batch.shader_key.fields.shader_pass;
//                 Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());

//                 if (shader == nullptr) {
//                     Log::Fatal("Failed to load forward pipeline shader");
//                 }

//                 shader->bind(command_buffer);

//                 UniformSetID uniform_sets[] = {
//                     renderer->per_frame_uniform_set,
//                     renderer->transform_material_set,
//                 };
//                 command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));
//                 command_buffer->set_push_constants(shader->pipeline_id, &push_constant, 1);

//                 for (auto &mesh_batch : batch.meshes) {
//                     DrawBatch(command_buffer, &mesh_batch, shader);
//                 }
//             }
//         };

//         if (render_batches.size() > 0) {
//             // Draw Opaque Object
//             PipelineState pipeline_state = {};
//             pipeline_state.render_state.fields.depth_test = true;
//             pipeline_state.render_state.fields.depth_write = false;
//             pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;
//             draw_batch(RENDERBATCH_TYPE_OPAQUE, pipeline_state);
//             /*
//             // Draw Transparent Object
//             pipeline_state.render_state.fields.cull_mode = CULL_MODE_NONE;
//             pipeline_state.render_state.fields.blend_mode = true;
//             pipeline_state.render_state.fields.depth_write = true;
//             for (auto &batch : render_batches) {
//                 draw_batch(RENDERBATCH_TYPE_TRANSPARENT, pipeline_state);
//             }
//             */
//         }
//         command_buffer->end_render_pass();

//         device->end_debug_utils_label(command_buffer);
//     }

//     ForwardPass::~ForwardPass() {
//     }
// } // namespace mirai
