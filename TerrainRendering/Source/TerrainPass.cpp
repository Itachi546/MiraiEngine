// #include "TerrainPass.hpp"
// #include "Common/Profiler.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Scene/Shader.hpp"
// #include "Scene/Scene.hpp"
// #include "Scene/Camera.hpp"
// #include "Common/FileUtils.hpp"
// #include "Device/Window.hpp"
// #include "Graphics/TextRenderManager.hpp"
// #include "Device/InputDevice.hpp"
// #include "Graphics/Renderer.hpp"

// namespace mirai {
//     TerrainPass::TerrainPass(uint32_t width, uint32_t height, uint32_t maxHeight, uint32_t cbt_depth) : FrameGraphRenderer("terrain_pass"),
//                                                                                                         width(width), height(height), cbt_depth(cbt_depth), maxHeight(maxHeight) {
//         device = RenderingDevice::get();
//     }

//     void TerrainPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         // Load heightmap
//         int width, height, n_channel;
//         auto data = utils::load_image16("Assets/kauai.png", &width, &height, &n_channel, 1);
//         if (data == nullptr)
//             Log::Error("Failed to load terrain heightmap");
//         ASSERT(n_channel == 1);
//         ASSERT(data != nullptr);

//         TextureDescription texture_desc = {
//             .create_flags = 0,
//             .width = (uint32_t)width,
//             .height = (uint32_t)height,
//             .depth = 1,
//             .mip_levels = 1,
//             .array_layers = 1,
//             .texture_type = TEXTURE_TYPE_2D,
//             .format = FORMAT_R16_UNORM,
//             .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
//         };
//         texture_heightmap = device->create_texture(&texture_desc, "heightmap");
//         rendering_utils::copy_texture_immediate(texture_heightmap, data.get(), width * height * sizeof(uint16_t));
//         data.reset();
//         data = nullptr;

//         // Allocate CBT Buffer
//         uint32_t allocation_size = (1 << (cbt_depth - 1));
//         BufferDescription buffer_desc = {
//             .size = allocation_size,
//             .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT,
//             .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
//         };
//         cbt_buffer = device->create_buffer(&buffer_desc, "CBT Node Buffer");

//         buffer_desc.size = sizeof(uint32_t) * 3;
//         buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT;
//         buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
//         cbt_dispatch_indirect_buffer = device->create_buffer(&buffer_desc, "CBT Dispatch Buffer");

//         // Size of VkCmdDrawIndirectCommand
//         buffer_desc.size = sizeof(uint32_t) * 4;
//         buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT;
//         cbt_draw_indirect_buffer = device->create_buffer(&buffer_desc, "CBT Draw Indirect Buffer");

//         SamplerDescription sampler_desc = SamplerDescription::create();
//         SamplerID heightmap_sampler = device->create_sampler(&sampler_desc);

//         UniformBinding vertex_bindings[] = {
//             {cbt_buffer},
//             {.resource_id = texture_heightmap, .texture_info = {.sampler = heightmap_sampler}},
//         };

//         UniformLayout vertex_layouts[] = {
//             {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_VERTEX},
//             {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_VERTEX},
//         };

//         // Vertex shader bindings
//         cbt_vert_set = device->create_uniform_set(&vertex_layouts[0], cast_u32(std::size(vertex_layouts)), 0, "cbt_vert_set");
//         device->update_uniform_set(cbt_vert_set, &vertex_bindings[0], cast_u32(std::size(vertex_bindings)));

//         std::vector<UniformLayout> cbt_init_layouts = {
//             {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//             /*{1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},*/
//         };

//         std::vector<UniformBinding> cbt_init_bindings = {
//             {cbt_buffer},
//             {cbt_dispatch_indirect_buffer},
//         };

//         // CBT Initialize Bindings
//         cbt_init_set = device->create_uniform_set(cbt_init_layouts.data(), cast_u32(cbt_init_layouts.size()), 0, "cbt_comp_set");
//         device->update_uniform_set(cbt_init_set, cbt_init_bindings.data(), cast_u32(cbt_init_bindings.size()));

//         std::vector<UniformLayout> cbt_update_subdivision_layouts = {
//             {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//         };

//         std::vector<UniformBinding> cbt_update_subdivision_bindings = {
//             {cbt_buffer},
//             {.resource_id = texture_heightmap, .texture_info = {.sampler = heightmap_sampler}},
//         };

//         // CBT Subdivision Bindings
//         cbt_subdivision_set = device->create_uniform_set(cbt_update_subdivision_layouts.data(), cast_u32(cbt_update_subdivision_layouts.size()), 0, "cbt_subdivision_set");
//         device->update_uniform_set(cbt_subdivision_set, cbt_update_subdivision_bindings.data(), cast_u32(cbt_update_subdivision_bindings.size()));

//         std::vector<UniformLayout> cbt_sum_reduction_layouts = {
//             {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//             {2, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
//         };

//         std::vector<UniformBinding> cbt_sum_reduction_bindings = {
//             {cbt_buffer},
//             {cbt_draw_indirect_buffer},
//             {cbt_dispatch_indirect_buffer},
//         };

//         // CBT Sum reduction prepass bindings
//         cbt_sum_reduction_prepass_set = device->create_uniform_set(cbt_sum_reduction_layouts.data(), 1, 0, "cbt_subdivision_set");
//         device->update_uniform_set(cbt_sum_reduction_prepass_set, cbt_sum_reduction_bindings.data(), 1);

//         // CBT Sum reduction bindings
//         cbt_sum_reduction_set = device->create_uniform_set(cbt_sum_reduction_layouts.data(), cast_u32(cbt_sum_reduction_layouts.size()), 0, "cbt_subdivision_set");
//         device->update_uniform_set(cbt_sum_reduction_set, cbt_sum_reduction_bindings.data(), cast_u32(cbt_sum_reduction_layouts.size()));

//         cbt_init_program = Shader::create_from_file("SPIRV/cbt_initialize.comp.spv", "cbt-init-program");
//         cbt_init_program->set_custom_bindings(&cbt_init_set, 1);

//         cbt_sum_reduction_program = Shader::create_from_file("SPIRV/cbt_sum_reduction.comp.spv", "cbt-sum-reduction");
//         cbt_sum_reduction_program->set_custom_bindings(&cbt_sum_reduction_set, 1);

//         cbt_sum_reduction_prepass_program = Shader::create_from_file("SPIRV/cbt_sum_reduction_prepass.comp.spv", "sum-reduction-prepass");
//         cbt_sum_reduction_prepass_program->set_custom_bindings(&cbt_sum_reduction_prepass_set, 1);

//         cbt_subdivision_program = Shader::create_from_file("SPIRV/cbt_subdivision.comp.spv", "cbt-subdivision");
//         cbt_subdivision_program->set_custom_bindings(&cbt_subdivision_set, 1);

//         // Initialize Terrain Shader
//         PipelineState pipeline_state{};
//         pipeline_state.render_state.fields.depth_test = true;
//         pipeline_state.render_state.fields.depth_write = true;

//         PipelineAttachmentInfo attachment_info{
//             .color_attachments_format = {
//                 FORMAT_B8G8R8A8_UNORM,
//             },
//             .has_depth_attachment = true,
//             .depth_attachment_format = {
//                 FORMAT_D32_SFLOAT,
//             },
//         };

//         terrain_shader = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/terrain.vert.spv", "SPIRV/terrain.frag.spv"}, "terrain-shader");
//         pipeline_state.render_state.fields.polygon_mode = POLYGON_MODE_LINE;
//         terrain_shader_wireframe = Shader::create_from_file(pipeline_state, attachment_info, {"SPIRV/terrain.vert.spv", "SPIRV/terrain.frag.spv"}, "terrain-shader-wireframe");

//         // Initialize CBT Buffer
//         init_at_depth(cbt_depth / 2);
//     }

//     void TerrainPass::init_at_depth(uint32_t initDepth) {
//         CommandBuffer *command_buffer = device->get_command_buffer(0);
//         command_buffer->begin();

//         device->begin_debug_utils_label(command_buffer, "Reset CBT Buffer", nullptr);

//         uint32_t push_constant_data[] = {cbt_depth, initDepth};
//         PushConstant push_constant = {
//             .data = &push_constant_data,
//             .offset = 0,
//             .size = sizeof(uint32_t) * cast_u32(std::size(push_constant_data)),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         command_buffer->set_push_constants(cbt_init_program->pipeline_id, &push_constant, 1);
//         cbt_init_program->bind(command_buffer);

//         uint32_t work_group_size = 1;
//         command_buffer->dispatch(1, 1, 1);

//         device->end_debug_utils_label(command_buffer);
//         device->submit_command_buffer_immediate(command_buffer);
//         command_buffer->wait();
//     }

//     void TerrainPass::compute_sum_reduction_prepass(CommandBuffer *command_buffer) {
//         device->begin_debug_utils_label(command_buffer, "CBT Sum Reduction Prepass", nullptr);
//         cbt_sum_reduction_prepass_program->bind(command_buffer);

//         // Proper buffer access transition from vertex shader input to compute shader
//         BufferBarrierInfo cbt_buffer_barrier_info[] = {
//             {
//                 .buffer_id = cbt_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//                 .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//             },
//         };

//         command_buffer->prepare_buffer(cbt_buffer_barrier_info, cast_u32(std::size(cbt_buffer_barrier_info)));

//         ScopedGpuProfiling(command_buffer, "Sum Reduction Prepass");

//         PushConstant push_constants = {
//             .data = &cbt_depth,
//             .offset = 0,
//             .size = sizeof(uint32_t),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         command_buffer->set_push_constants(cbt_sum_reduction_prepass_program->pipeline_id, &push_constants, 1);
//         uint32_t local_work_size = rendering_utils::get_workgroup_size((1 << cbt_depth) / 32, 256);
//         command_buffer->dispatch(local_work_size, 1, 1);
//         device->end_debug_utils_label(command_buffer);
//     }

//     void TerrainPass::compute_sum_reduction(CommandBuffer *command_buffer) {
//         device->begin_debug_utils_label(command_buffer, "CBT Sum Reduction", nullptr);
//         // cbt_depth = 8, so we start with 7 and
//         uint32_t num_dispatches = enable_sumreduction_prepass ? cbt_depth - 6 : cbt_depth - 1;
//         cbt_sum_reduction_program->bind(command_buffer);

//         // Proper buffer access transition from vertex shader input to compute shader
//         BufferBarrierInfo cbt_buffer_barrier_info[] = {
//             {
//                 .buffer_id = cbt_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//                 .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//             },
//             {
//                 .buffer_id = cbt_draw_indirect_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
//                 .src_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
//                 .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_WRITE,
//             },
//             {
//                 .buffer_id = cbt_dispatch_indirect_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
//                 .src_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
//                 .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_WRITE,
//             },
//         };

//         ScopedGpuProfiling(command_buffer, "Sum Reduction");
//         for (int level = num_dispatches; level >= 0; level--) {
//             command_buffer->prepare_buffer(cbt_buffer_barrier_info, cast_u32(std::size(cbt_buffer_barrier_info)));
//             PushConstant push_constants = {
//                 .data = &level,
//                 .offset = 0,
//                 .size = sizeof(uint32_t),
//                 .shader_stage = SHADER_STAGE_COMPUTE,
//             };
//             command_buffer->set_push_constants(cbt_sum_reduction_program->pipeline_id, &push_constants, 1);
//             uint32_t local_work_size = rendering_utils::get_workgroup_size(1 << level, 256);
//             command_buffer->dispatch(local_work_size, 1, 1);
//         }
//         device->end_debug_utils_label(command_buffer);
//     }

//     void TerrainPass::update_subdivision(CommandBuffer *command_buffer, Camera *camera) {
//         ScopedGpuProfiling(command_buffer, "Update Subdivision");
//         device->begin_debug_utils_label(command_buffer, "CBT Update Subdivision", nullptr);

//         if (!freeze_frustum) {
//             push_constant_data.VP = camera->get_view_projection_transform();
//             Frustum &frustum = camera->get_frustum();
//             for (int i = 0; i < 6; ++i) {
//                 push_constant_data.frustum_planes[i] = glm::vec4(frustum.planes[i].normal, frustum.planes[i].distance);
//             }
//         }
//         push_constant_data.subdivision_info = glm::vec4(subdivision_mode, lod_factor, 0.0f, 0.0f);
//         push_constant_data.dims = glm::vec4(float(width), float(height), float(maxHeight), float(enable_sumreduction_prepass));

//         cbt_subdivision_program->bind(command_buffer);

//         PushConstant push_constant = {
//             .data = &push_constant_data,
//             .offset = 0,
//             .size = sizeof(push_constant_data),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         command_buffer->set_push_constants(cbt_subdivision_program->pipeline_id, &push_constant, 1);
//         // Proper buffer access transition from vertex shader input to compute shader
//         BufferBarrierInfo cbt_buffer_barrier_info[] = {
//             {
//                 .buffer_id = cbt_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_READ,
//                 .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//             },
//             {
//                 .buffer_id = cbt_dispatch_indirect_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_WRITE,
//                 .dst_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
//                 .dst_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
//             },
//         };

//         command_buffer->prepare_buffer(cbt_buffer_barrier_info, 2);
//         command_buffer->dispatch_indirect(cbt_dispatch_indirect_buffer, 0);
//         device->end_debug_utils_label(command_buffer);
//     }

//     void TerrainPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         subdivision_mode = 1.0f - subdivision_mode;

//         Camera *camera = renderer->get_scene()->get_camera();

//         uint32_t screenWidth, screenHeight;
//         Window::get()->get_size(&screenWidth, &screenHeight);

//         const uint32_t gpuSubdivision = 2;
//         const float pixelLengthTarget = 3.0f;
//         float tmp = 2.0f * tan(glm::radians(camera->get_fov()) / 2.0f) / screenHeight * (1 << gpuSubdivision) * pixelLengthTarget;
//         lod_factor = -2.0f * std::log2(tmp) + 2.0f;

//         TextRenderer *text_renderer = TextRenderManager::get()->get_default();

//         float startX = screenWidth * 0.85f;
//         float startY = 20.0f;
//         text_renderer->AddText("Freeze Frustum: " + std::string(freeze_frustum ? "true" : "false"), glm::vec2(startX, startY), 14);
//         text_renderer->AddText("Wireframe Mode: " + std::string(enable_wireframe ? "true" : "false"), glm::vec2(startX, startY + 18.0f), 14);
//         text_renderer->AddText("Sum Reduction Prepass: " + std::string(enable_sumreduction_prepass ? "true" : "false"), glm::vec2(startX, startY + 36.0f), 14);

//         if (Input::get()->was_down(Key::KB_SPACE))
//             enable_wireframe = !enable_wireframe;
//         if (Input::get()->was_down(Key::KB_F))
//             freeze_frustum = !freeze_frustum;
//         if (Input::get()->was_down(Key::KB_P))
//             enable_sumreduction_prepass = !enable_sumreduction_prepass;
//     }

//     void TerrainPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         device->begin_debug_utils_label(command_buffer, "TerrainPass", nullptr);

//         Scene *scene = renderer->get_scene();
//         update_subdivision(command_buffer, scene->get_camera());

//         if (enable_sumreduction_prepass)
//             compute_sum_reduction_prepass(command_buffer);

//         compute_sum_reduction(command_buffer);

//         ScopedGpuProfiling(command_buffer, "Render Terrain");
//         // Prepare cbt_buffer for vertex read
//         BufferBarrierInfo barrier_infos[] = {
//             {
//                 .buffer_id = cbt_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
//                 .dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
//                 .dst_access_mask = ACCESS_FLAG_SHADER_READ,
//             },
//             {
//                 .buffer_id = cbt_draw_indirect_buffer,
//                 .offset = 0,
//                 .size = UINT64_MAX,
//                 .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .src_access_mask = ACCESS_FLAG_SHADER_WRITE,
//                 .dst_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
//                 .dst_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
//             },

//         };

//         command_buffer->prepare_buffer(barrier_infos, cast_u32(std::size(barrier_infos)));

//         command_buffer->begin_render_pass(node, frame_graph);

//         auto shader = enable_wireframe ? terrain_shader_wireframe : terrain_shader;

//         UniformSetID uniform_sets[] = {
//             cbt_vert_set,
//         };

//         shader->bind(command_buffer);
//         command_buffer->set_uniform_sets(shader->pipeline_id, uniform_sets, cast_u32(std::size(uniform_sets)));

//         struct TerrainPushConstant {
//             glm::mat4 VP;
//             glm::vec4 dims;
//         } push_constant_data;
//         push_constant_data.VP = scene->get_camera()->get_view_projection_transform();
//         push_constant_data.dims = {float(width), float(height), float(maxHeight), float(enable_sumreduction_prepass)};
//         PushConstant push_constant = {
//             .data = &push_constant_data,
//             .offset = 0,
//             .size = sizeof(push_constant_data),
//             .shader_stage = SHADER_STAGE_VERTEX,
//         };

//         command_buffer->set_push_constants(shader->pipeline_id, &push_constant, 1);
//         command_buffer->draw_indirect(cbt_draw_indirect_buffer, 0, 1, sizeof(uint32_t) * 4);
//         command_buffer->end_render_pass();

//         device->end_debug_utils_label(command_buffer);
//     }

//     TerrainPass::~TerrainPass() {
//         device->destroy_textures(&texture_heightmap, 1);
//         BufferID buffers[] = {cbt_buffer, cbt_dispatch_indirect_buffer, cbt_draw_indirect_buffer};
//         device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
//     }

// } // namespace mirai