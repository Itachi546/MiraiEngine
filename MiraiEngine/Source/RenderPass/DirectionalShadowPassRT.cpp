// #include "DirectionalShadowPassRT.hpp"
// #include "Scene/Camera.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Scene/Shader.hpp"
// #include "Engine/Profiler.hpp"
// #include "Graphics/Renderer.hpp"

// namespace mirai {
//     void DirectionalShadowPassRT::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         dir_shadow_shader = Shader::create_from_file("SPIRV/rt-directional-shadow.comp.spv", "rt-shadow-shader");
//         blur_shader = Shader::create_from_file("SPIRV/shadow-blur.comp.spv", "rt-shadow-blur-shader");

//         UniformLayout rt_layouts[] = {
//             {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//             {2, BINDING_TYPE_ACCELERATION_STRUCTURE, SHADER_STAGE_COMPUTE},
//         };

//         ASSERT(node->outputs.size() == 1);
//         ASSERT(node->inputs.size() == 1);

//         TextureID rt_shadow_texture = frame_graph->get_resource(node->outputs[0])->handle;
//         TextureID depth_texture = frame_graph->get_resource(node->inputs[0])->handle;

//         SamplerDescription sampler_desc = SamplerDescription::create();
//         SamplerID default_sampler = device->create_sampler(&sampler_desc);

//         sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_NEAREST;
//         SamplerID depth_sampler = device->create_sampler(&sampler_desc);

//         UniformBinding rt_bindings[] = {
//             {.resource_id = rt_shadow_texture, .texture_info = {.sampler = default_sampler}},
//             {.resource_id = depth_texture, .texture_info = {.sampler = depth_sampler}},
//             // Acceleration structure is global and populated by the vulkan device
//             {.resource_id = K_INVALID_ID},
//         };

//         rt_uniform_set = device->create_uniform_set(rt_layouts, cast_u32(std::size(rt_layouts)), 0, "rt_shadow_set");
//         device->update_uniform_set(rt_uniform_set, rt_bindings, cast_u32(std::size(rt_bindings)));

//         // Create blur intermediate texture
//         TextureDescription texture_desc = {
//             .create_flags = 0,
//             .width = node->width,
//             .height = node->height,
//             .depth = 1,
//             .mip_levels = 1,
//             .array_layers = 1,
//             .texture_type = TEXTURE_TYPE_2D,
//             .format = FORMAT_R8_UNORM,
//             .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
//         };

//         blur_intermediate_texture = device->create_texture(&texture_desc, "blur_intermediate_texture");

//         UniformLayout layouts[] = {
//             {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//             {2, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//         };

//         UniformBinding bindings[] = {
//             {.resource_id = blur_intermediate_texture},
//             {
//                 .resource_id = rt_shadow_texture,
//                 .texture_info = {.sampler = default_sampler},
//             },
//             {
//                 .resource_id = depth_texture,
//                 .texture_info = {.sampler = depth_sampler},
//             },
//         };

//         uint32_t binding_count = cast_u32(std::size(bindings));
//         blur_uniform_set_x = device->create_uniform_set(layouts, binding_count, 0, "rt_shadow_blur_set");
//         device->update_uniform_set(blur_uniform_set_x, bindings, binding_count);

//         bindings[0].resource_id = rt_shadow_texture;
//         bindings[1].resource_id = blur_intermediate_texture;
//         blur_uniform_set_y = device->create_uniform_set(layouts, binding_count, 0, "rt_shadow_blur_set");
//         device->update_uniform_set(blur_uniform_set_y, bindings, binding_count);
//     }

//     void DirectionalShadowPassRT::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         ScopedCpuProfiling("RT Shadow Pass");
//         Scene *scene = renderer->get_scene();

//         device->begin_debug_utils_label(command_buffer, "RT Shadow Pass", nullptr);
//         render_shadow(command_buffer, frame_graph, node, scene);
//         device->end_debug_utils_label(command_buffer);

//         device->begin_debug_utils_label(command_buffer, "RT Shadow Blur Pass", nullptr);
//         blur_shadow(command_buffer, frame_graph, node, scene);
//         device->end_debug_utils_label(command_buffer);
//     }

//     void DirectionalShadowPassRT::render_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
//         ScopedGpuProfiling(command_buffer, "RT Shadow Render Pass");
//         struct ShaderData {
//             glm::mat4 invVP;
//             glm::vec3 light_direction;
//             float width;
//             float height;
//         } shader_data;

//         Camera *camera = scene->get_camera();
//         shader_data.invVP = camera->get_inv_view_projection_transform();
//         shader_data.light_direction = scene->get_sun()->get_direction();
//         shader_data.width = cast_float(node->width);
//         shader_data.height = cast_float(node->height);

//         command_buffer->begin_compute_pass(node, frame_graph);
//         PushConstant push_constant = {
//             .data = &shader_data,
//             .offset = 0,
//             .size = sizeof(shader_data),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         dir_shadow_shader->bind(command_buffer);
//         PipelineID pipeline_id = dir_shadow_shader->pipeline_id;
//         command_buffer->set_uniform_sets(pipeline_id, &rt_uniform_set, 1);
//         command_buffer->set_push_constants(pipeline_id, &push_constant, 1);

//         uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
//         uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

//         command_buffer->dispatch(work_size_x, work_size_y, 1);
//     }

//     void DirectionalShadowPassRT::blur_shadow(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
//         ScopedGpuProfiling(command_buffer, "RT Shadow Blur Pass");
//         TextureID rt_shadow_texture = frame_graph->get_resource(node->outputs[0])->handle;

//         // Transition layout
//         TextureBarrierInfo barrier_infos[] = {
//             {
//                 .texture_id = rt_shadow_texture,
//                 .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .access_mask = ACCESS_FLAG_SHADER_READ,
//                 .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//             },
//             {
//                 .texture_id = blur_intermediate_texture,
//                 .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .access_mask = ACCESS_FLAG_SHADER_WRITE,
//                 .layout = IMAGE_LAYOUT_GENERAL,
//             },
//         };

//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         float blur_data[] = {cast_float(node->width), cast_float(node->height), 0.0f, scene->get_camera()->get_near_plane()};
//         uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
//         uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);
//         PushConstant push_constant = {
//             .data = blur_data,
//             .offset = 0,
//             .size = sizeof(blur_data),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         // Blur in X-direction

//         blur_shader->bind(command_buffer);
//         PipelineID pipeline_id = blur_shader->pipeline_id;

//         command_buffer->set_uniform_sets(pipeline_id, &blur_uniform_set_x, 1);
//         command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
//         command_buffer->dispatch(work_size_x, work_size_y, 1);

//         // Blur in Y-Direction
//         blur_data[2] = 1.0f;

//         barrier_infos[0].access_mask = ACCESS_FLAG_SHADER_WRITE;
//         barrier_infos[0].layout = IMAGE_LAYOUT_GENERAL;

//         barrier_infos[1].access_mask = ACCESS_FLAG_SHADER_READ;
//         barrier_infos[1].layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         command_buffer->set_uniform_sets(pipeline_id, &blur_uniform_set_y, 1);
//         command_buffer->set_push_constants(pipeline_id, &push_constant, 1);
//         blur_shader->bind(command_buffer);
//         command_buffer->dispatch(work_size_x, work_size_y, 1);
//     }

//     DirectionalShadowPassRT::~DirectionalShadowPassRT() {
//         if (blur_intermediate_texture.is_valid())
//             device->destroy_textures(&blur_intermediate_texture, 1);
//     }
// } // namespace mirai