// #include "TAAResolvePass.hpp"
// #include "Scene/Shader.hpp"
// #include "Engine/Profiler.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"

// namespace mirai {

//     void TAAResolvePass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         // Create TAA history texture
//         TextureDescription texture_desc = {
//             .create_flags = 0,
//             // @TODO Fix this later
//             .width = taa_texture_width,
//             .height = taa_texture_height,
//             .depth = 1,
//             .mip_levels = 1,
//             .array_layers = 1,
//             .texture_type = TEXTURE_TYPE_2D,
//             .format = FORMAT_R16G16B16A16_SFLOAT,
//             .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
//         };

//         taa_history_textures[0] = device->create_texture(&texture_desc, "taa_history_0");
//         taa_history_textures[1] = device->create_texture(&texture_desc, "taa_history_1");

//         UniformLayout layouts[] = {
//             {.binding = 0, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
//             {.binding = 1, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
//             {.binding = 2, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
//             {.binding = 3, .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER, .shader_stage = SHADER_STAGE_COMPUTE},
//             {.binding = 4, .binding_type = BINDING_TYPE_STORAGE_IMAGE, .shader_stage = SHADER_STAGE_COMPUTE},
//         };

//         uniform_set[0] = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "taa_resolve_set0");
//         uniform_set[1] = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "taa_resolve_set1");

//         FrameGraphResource *gbuffer_lighting = frame_graph->get_resource("gbuffer_lighting");
//         ASSERT(gbuffer_lighting != nullptr);

//         FrameGraphResource *gbuffer_depth = frame_graph->get_resource("gbuffer_depth");
//         ASSERT(gbuffer_depth != nullptr);

//         FrameGraphResource *gbuffer_velocity = frame_graph->get_resource("gbuffer_velocity");
//         ASSERT(gbuffer_velocity != nullptr);

//         SamplerDescription sampler_desc = SamplerDescription::create();
//         SamplerID sampler = device->create_sampler(&sampler_desc);

//         sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_NEAREST;
//         SamplerID depth_sampler = device->create_sampler(&sampler_desc);

//         UniformBinding bindings[] = {
//             {.resource_id = gbuffer_lighting->handle, .texture_info = {.sampler = sampler}},
//             {.resource_id = taa_history_textures[1], .texture_info = {.sampler = sampler}},
//             {.resource_id = gbuffer_depth->handle, .texture_info = {.sampler = depth_sampler}},
//             {.resource_id = gbuffer_velocity->handle, .texture_info = {.sampler = sampler}},
//             {.resource_id = taa_history_textures[0]},
//         };

//         device->update_uniform_set(uniform_set[0], bindings, cast_u32(std::size(bindings)));

//         bindings[1].resource_id = taa_history_textures[0];
//         bindings[4].resource_id = taa_history_textures[1];
//         device->update_uniform_set(uniform_set[1], bindings, cast_u32(std::size(bindings)));

//         shader = Shader::create_from_file("SPIRV/taa-resolve.comp.spv", "taa-resolve-shader");
//         // copy_texture_shader = Shader::create_from_file("SPIRV/copy-texture.comp.spv", "taa-copy-texture-shader");
//     }

//     void TAAResolvePass::reset_history_texture(CommandBuffer *command_buffer, FrameGraph *frame_graph) {
//         // Should copy the current rendertarget to the TAA history texture
//         FrameGraphResource *gbuffer_lighting = frame_graph->get_resource("gbuffer_lighting");
//         ASSERT(gbuffer_lighting != nullptr);

//         TextureBarrierInfo barrier_infos[] = {
//             {
//                 .texture_id = gbuffer_lighting->handle,
//                 .stage_mask = PIPELINE_STAGE_TRANSFER_BIT,
//                 .access_mask = ACCESS_FLAG_TRANSFER_READ,
//                 .layout = IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//             },
//             {
//                 .texture_id = taa_history_textures[current_taa_texture],
//                 .stage_mask = PIPELINE_STAGE_TRANSFER_BIT,
//                 .access_mask = ACCESS_FLAG_TRANSFER_WRITE,
//                 .layout = IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//             },
//         };

//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         command_buffer->copy_texture(taa_history_textures[current_taa_texture], gbuffer_lighting->handle, taa_texture_width, taa_texture_height);

//         should_reset_history_texture = false;
//     }

//     void TAAResolvePass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         current_taa_texture = 1 - current_taa_texture;

//         ScopedGpuProfiling(command_buffer, "TAAResolve");
//         device->begin_debug_utils_label(command_buffer, "TAA Resolve", nullptr);

//         command_buffer->begin_compute_pass(node, frame_graph);

//         FrameGraphResource *gbuffer_lighting = frame_graph->get_resource("gbuffer_lighting");
//         ASSERT(gbuffer_lighting != nullptr);

//         FrameGraphResource *taa_history = frame_graph->get_resource("taa_output");
//         ASSERT(taa_history != nullptr);

//         FrameGraphResource *gbuffer_depth = frame_graph->get_resource("gbuffer_depth");
//         ASSERT(gbuffer_depth != nullptr);

//         int flags = 0;
//         flags = (flags | int(enable_taa)) |
//                 ((int(should_sample_motion_vector) << 1)) |
//                 ((int(enable_temporal_filtering) << 2)) |
//                 ((int(enable_taa_simple) << 3)) |
//                 ((int(should_enable_min_depth) << 4)) |
//                 ((int(should_enable_history_sampling) << 5));

//         // Copy output texture to TAA history
//         int push_constant_data[] = {
//             cast_int(gbuffer_lighting->resource_info.width),
//             cast_int(gbuffer_lighting->resource_info.height),
//             flags,
//             0};

//         PushConstant push_constants = {
//             .data = push_constant_data,
//             .offset = 0,
//             .size = sizeof(int) * 4,
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         // Copy the deferred texture output to TAA history if it is first frame
//         if (should_reset_history_texture) {
//             reset_history_texture(command_buffer, frame_graph);
//         } else {
//             TextureBarrierInfo barrier_infos[] = {
//                 {
//                     .texture_id = taa_history_textures[current_taa_texture],
//                     .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                     .access_mask = ACCESS_FLAG_SHADER_WRITE,
//                     .layout = IMAGE_LAYOUT_GENERAL,
//                 },
//                 {
//                     .texture_id = taa_history_textures[1 - current_taa_texture],
//                     .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                     .access_mask = ACCESS_FLAG_SHADER_READ,
//                     .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//                 },
//             };

//             command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//             shader->bind(command_buffer);
//             command_buffer->set_uniform_sets(shader->pipeline_id, &uniform_set[current_taa_texture], 1);
//             command_buffer->set_push_constants(shader->pipeline_id, &push_constants, 1);

//             uint32_t work_size_x = rendering_utils::get_workgroup_size(taa_texture_width, 32);
//             uint32_t work_size_y = rendering_utils::get_workgroup_size(taa_texture_height, 32);
//             command_buffer->dispatch(work_size_x, work_size_y, 1);
//         }

//         TextureBarrierInfo barrier_infos[] = {
//             {
//                 .texture_id = taa_history_textures[current_taa_texture],
//                 .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//                 .access_mask = ACCESS_FLAG_SHADER_READ,
//                 .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//             },
//         };

//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         // Inject the valid texture in the frame graph every frame
//         // This is just a hack
//         frame_graph->get_resource("taa_output")->handle = taa_history_textures[current_taa_texture];

//         device->end_debug_utils_label(command_buffer);
//     }

//     TAAResolvePass::~TAAResolvePass() {
//         if (taa_history_textures[0].is_valid())
//             device->destroy_textures(taa_history_textures, 2);
//     }

// } // namespace mirai