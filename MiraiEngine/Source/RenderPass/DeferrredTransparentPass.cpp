// #include "DeferredTransparentPass.hpp"
// #include "Scene/RenderBatch.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Scene/ShaderHashMap.hpp"
// #include "Engine/Profiler.hpp"
// #include "Graphics/Renderer.hpp"

// namespace mirai {
//     DeferredTransparentPass::DeferredTransparentPass() : FrameGraphRenderer("deferred_transparent_pass") {
//     }

//     void DeferredTransparentPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         ASSERT(node != nullptr);
//         ScopedGpuProfiling(command_buffer, "Deferred TransparentPass");

//         device->begin_debug_utils_label(command_buffer, "DeferredTransparent", nullptr);

//         std::vector<RenderBatch> &render_batches = renderer->main_render_batches;
//         auto draw_batch = [&](RenderBatchType render_batch_type, PipelineState &pipeline_state) {
//             for (auto &batch : render_batches) {
//                 if (batch.batch_type != render_batch_type)
//                     continue;
//                 pipeline_state.custom_shader_id = batch.shader_key.fields.custom_shader_id;
//                 pipeline_state.render_state.fields.pass_mode = batch.shader_key.fields.shader_pass;
//                 Shader *shader = ShaderHashMap::get()->get(pipeline_state.get_hash());

//                 if (shader == nullptr) {
//                     Log::Fatal("Failed to load deferred transparent pipeline shader");
//                 }

//                 shader->bind(command_buffer);

//                 UniformSetID uniform_sets[] = {
//                     renderer->per_frame_uniform_set,
//                     renderer->transform_material_set,
//                 };
//                 command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

//                 for (auto &mesh_batch : batch.meshes) {
//                     DrawBatch(command_buffer, &mesh_batch, shader);
//                 }
//             }
//         };

//         command_buffer->begin_render_pass(node, frame_graph);
//         if (render_batches.size() > 0) {
//             PipelineState pipeline_state = {};
//             pipeline_state.render_state.fields.depth_test = true;
//             pipeline_state.render_state.fields.depth_write = false;
//             pipeline_state.render_state.fields.draw_mode = DRAWMODE_INDEXED_INDIRECT;
//             pipeline_state.render_state.fields.cull_mode = CULL_MODE_BACK;
//             pipeline_state.render_state.fields.blend_mode = true;
//             draw_batch(RENDERBATCH_TYPE_TRANSPARENT, pipeline_state);
//         }
//         command_buffer->end_render_pass();

//         device->end_debug_utils_label(command_buffer);
//     }

//     DeferredTransparentPass::~DeferredTransparentPass() {
//     }
// } // namespace mirai