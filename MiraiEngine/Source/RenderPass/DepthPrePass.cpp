#include "DepthPrePass.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "RenderPassData.hpp"
#include "Engine/AppSettings.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

namespace mirai {
    DepthPrePass::DepthPrePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        RenderingDevice *device = RenderingDevice::get();

        UniformLayout transform_material_layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
        };

        UniformSetID transform_set = device->create_uniform_set(transform_material_layouts, cast_u32(std::size(transform_material_layouts)), 1);
        Renderer *renderer = Renderer::get();
        UniformBinding bindings[] = {
            {.resource_id = renderer->global_transform_buffer},
        };
        device->update_uniform_set(transform_set, bindings, cast_u32(std::size(bindings)));

        frame_graph->add_callback_pass<DepthPrePassData>(
            "DepthPrePass",
            [=](FrameGraph::FrameGraphBuilder &builder, DepthPrePassData &data) {
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
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
                data.transform_set = transform_set;
                board->add<DepthPrePassData>(data);
            },
            [](const DepthPrePassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;
                Renderer *renderer = ctx->renderer;

                PipelineState pipeline_state;
                pipeline_state.render_state.fields.depth_test = true;
                pipeline_state.render_state.fields.depth_write = true;
                pipeline_state.render_state.fields.pass_mode = SHADER_PASS_DEPTH_PREPASS;
                pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;

                ScopedGpuProfiling(command_buffer, "DepthPrePass");
                command_buffer->begin_gpu_debug_label("DepthPrePass");

                std::vector<RenderBatch> &render_batches = renderer->main_render_batches;

                std::vector<UniformSetID> uniform_sets{
                    renderer->vt_per_frame_uniform_set,
                    data.transform_set,
                };

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

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

                // Draw Opaque meshes
                Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
                if (shader == nullptr) {
                    Log::Fatal("Failed to load pipeline for depth-prepass");
                }

                DrawBatch(command_buffer, render_batches, {
                                                              .batch_type = RENDERBATCH_TYPE_OPAQUE,
                                                              .shader = shader,
                                                              .bindings = uniform_sets,
                                                              .push_constants = {},
                                                          });

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai

// #include "DepthPrePass.hpp"
// #include "Scene/ShaderHashMap.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Engine/Profiler.hpp"
// #include "Graphics/Renderer.hpp"
// namespace mirai {
//     DepthPrePass::DepthPrePass() : FrameGraphRenderer("depth_prepass") {
//     }

//     void DepthPrePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         // Mesh Data
//         PipelineState pipeline_state;
//         pipeline_state.render_state.fields.depth_test = true;
//         pipeline_state.render_state.fields.depth_write = true;
//         pipeline_state.render_state.fields.pass_mode = SHADER_PASS_DEPTH_PREPASS;
//         pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;

//         shader = ShaderHashMap::get()->get(pipeline_state.get_hash());
//         if (shader == nullptr) {
//             Log::Fatal("Failed to load pipeline for depth-prepass");
//         }

//         UniformLayout transform_material_layouts[] = {
//             {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
//         };
//         transform_set = device->create_uniform_set(transform_material_layouts, cast_u32(std::size(transform_material_layouts)), 1);

//         UniformBinding bindings[] = {
//             {.resource_id = renderer->global_transform_buffer},
//         };
//         device->update_uniform_set(transform_set, bindings, cast_u32(std::size(bindings)));
//     }

//     void DepthPrePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         ScopedGpuProfiling(command_buffer, "DepthPrePass");

//         device->begin_debug_utils_label(command_buffer, "Depth PrePass", nullptr);

//         command_buffer->begin_render_pass(node, frame_graph);

//         shader->bind(command_buffer);

//         UniformSetID uniform_sets[] = {
//             renderer->vt_per_frame_uniform_set,
//             transform_set,
//         };
//         command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

//         std::vector<RenderBatch> &render_batches = renderer->main_render_batches;
//         if (render_batches.size() > 0) {
//             for (auto &batch : render_batches) {
//                 if (batch.batch_type == RENDERBATCH_TYPE_TRANSPARENT)
//                     continue;
//                 for (auto &mesh_batch : batch.meshes)
//                     DrawBatch(command_buffer, &mesh_batch, shader, 3);
//             }
//         }

//         command_buffer->end_render_pass();

//         device->end_debug_utils_label(command_buffer);
//     }

//     DepthPrePass::~DepthPrePass() {
//     }
// } // namespace mirai