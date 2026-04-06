#include "ForwardPass.hpp"

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"

namespace mirai {

    ForwardPass::ForwardPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

        frame_graph->add_callback_pass<ForwardPassData>(
            "ForwardPass",
            [board](FrameGraph::FrameGraphBuilder &builder, ForwardPassData &data) {
                RenderingDevice *device = RenderingDevice::get();
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                data.output = builder.create_texture("LightingTexture", {
                                                                            .create_flags = 0,
                                                                            .width = width,
                                                                            .height = height,
                                                                            .depth = 1,
                                                                            .mip_levels = 1,
                                                                            .array_layers = 1,
                                                                            .texture_type = TEXTURE_TYPE_2D,
                                                                            .format = FORMAT_B8G8R8A8_UNORM,
                                                                            .usage_flags = TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
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

                data.registry = ShaderRegistryMap::get()->get_registry(PASS_MODE_FORWARD);
                board->add<ForwardPassData>(data);
            },
            [](const ForwardPassData &data, FrameGraphPassResource &pass_resource, void *context) {

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
