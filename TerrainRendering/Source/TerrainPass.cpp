#include "TerrainPass.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Common/FileUtils.hpp"
#include "Device/Window.hpp"
#include "Graphics/TextRenderManager.hpp"

#define CBT_IMPLEMENTATION
#include "CBT.hpp"

namespace mirai {
    TerrainPass::TerrainPass(uint32_t width, uint32_t height, uint32_t cbt_depth) : FrameGraphRenderer("terrain_pass"),
                                                                                    width(width), height(height), cbt_depth(cbt_depth) {
        device = RenderingDevice::get();
    }

    void TerrainPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
        // Load heightmap
        int width, height, n_channel;
        uint16_t *data = utils::load_image16("Assets/botw.png", &width, &height, &n_channel, 1);
        if (data == nullptr)
            Log::Error("Failed to load terrain heightmap");
        ASSERT(n_channel == 1);
        ASSERT(data != nullptr);

        TextureDescription texture_desc = {
            .create_flags = 0,
            .width = (uint32_t)width,
            .height = (uint32_t)height,
            .depth = 1,
            .mip_levels = 1,
            .array_layers = 1,
            .texture_type = TEXTURE_TYPE_2D,
            .format = FORMAT_R16_UNORM,
            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_DST_BIT,
        };
        texture_heightmap = device->create_texture(&texture_desc, "heightmap");
        rendering_utils::copy_texture_immediate(texture_heightmap, data, width * height * sizeof(uint16_t));

        // Allocate CBT Buffer
        uint32_t allocation_size = (1 << (cbt_depth - 1));
        BufferDescription buffer_desc = {
            .size = allocation_size,
            .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
        };
        cbt_buffer = device->create_buffer(&buffer_desc, "CBT Node Buffer");

        buffer_desc.size = sizeof(uint32_t) * 2;
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_CPU;
        cbt_leaf_count_buffer = device->create_buffer(&buffer_desc, "CBT Indirect Buffer");
        cbt_leaf_count_ptr = reinterpret_cast<uint32_t *>(device->map_buffer(cbt_leaf_count_buffer));

        // Size of VkCmdDrawIndirectCommand
        buffer_desc.size = sizeof(uint32_t) * 4;
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT;
        cbt_draw_indirect_buffer = device->create_buffer(&buffer_desc, "CBT Draw Indirect Buffer");

        SamplerDescription sampler_desc = SamplerDescription::create();
        SamplerID heightmap_sampler = device->create_sampler(&sampler_desc);

        UniformBinding vertex_bindings[] = {
            {cbt_buffer},
            {.resource_id = texture_heightmap, .texture_info = {.sampler = heightmap_sampler}},
        };

        UniformLayout vertex_layouts[] = {
            {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_VERTEX},
            {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_VERTEX},
        };

        cbt_vert_set = device->create_uniform_set(&vertex_layouts[0], cast_u32(std::size(vertex_layouts)), 1, "cbt_vert_set");
        device->update_uniform_set(cbt_vert_set, &vertex_bindings[0], cast_u32(std::size(vertex_bindings)));

        std::vector<UniformBinding> compute_bindings = {
            {cbt_buffer},
            {cbt_leaf_count_buffer},
            {.resource_id = texture_heightmap, .texture_info = {.sampler = heightmap_sampler}},
        };

        std::vector<UniformLayout> compute_layouts = {
            {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
            {1, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
            {2, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
        };

        cbt_init_set = device->create_uniform_set(compute_layouts.data(), 2, 0, "cbt_comp_set");
        device->update_uniform_set(cbt_init_set, compute_bindings.data(), 2);

        cbt_subdivision_set = device->create_uniform_set(compute_layouts.data(), 3, 0, "cbt_subdivision_set");
        device->update_uniform_set(cbt_subdivision_set, compute_bindings.data(), 3);

        compute_bindings[2] = {cbt_draw_indirect_buffer};
        compute_layouts[2] = {2, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE};

        cbt_sum_reduction_set = device->create_uniform_set(compute_layouts.data(), 3, 0, "cbt_subdivision_set");
        device->update_uniform_set(cbt_sum_reduction_set, compute_bindings.data(), 3);

        cbt_init_program = std::make_unique<ComputeShader>("cbt_initialize");
        cbt_init_program->create_from_file("SPIRV/cbt_initialize.comp.spv");
        cbt_init_program->set_uniform_sets(&cbt_init_set, 1);

        cbt_sum_reduction_program = std::make_unique<ComputeShader>("cbt_sumreduction");
        cbt_sum_reduction_program->create_from_file("SPIRV/cbt_sum_reduction.comp.spv");
        cbt_sum_reduction_program->set_uniform_sets(&cbt_sum_reduction_set, 1);

        cbt_sum_reduction_prepass_program = std::make_unique<ComputeShader>("cbt_sumreduction_prepass");
        cbt_sum_reduction_prepass_program->create_from_file("SPIRV/cbt_sum_reduction_prepass.comp.spv");
        cbt_sum_reduction_prepass_program->set_uniform_sets(&cbt_sum_reduction_set, 1);

        cbt_subdivision_program = std::make_unique<ComputeShader>("cbt_subdivision");
        cbt_subdivision_program->create_from_file("SPIRV/cbt_subdivision.comp.spv");
        cbt_subdivision_program->set_uniform_sets(&cbt_subdivision_set, 1);

        // Initialize Terrain Shader
        terrain_shader = std::make_unique<ShaderMaterial>("Terrain Shader");
        terrain_shader->create_from_file({"SPIRV/terrain.vert.spv", "SPIRV/terrain.frag.spv"}, {
                                                                                                   .cull_mode = CULL_MODE_NONE,
                                                                                                   .depth_test = true,
                                                                                                   .depth_write = true,
                                                                                                   .polygon_mode = POLYGON_MODE_FILL,
                                                                                               });

        // Initialize CBT Buffer
        init_at_depth(7);
    }

    void TerrainPass::init_at_depth(uint32_t initDepth) {
        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();

        device->begin_debug_utils_label(command_buffer, "Reset CBT Buffer", nullptr);

        uint32_t push_constant_data[] = {cbt_depth, initDepth};
        PushConstant push_constant = {
            .data = &push_constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(uint32_t) * cast_u32(std::size(push_constant_data)),
            .offset = 0,
        };

        cbt_init_program->set_push_constant(&push_constant, 1);
        cbt_init_program->bind(command_buffer);

        uint32_t work_group_size = 1;
        command_buffer->dispatch(1, 1, 1);

        device->end_debug_utils_label(command_buffer);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
    }

    void TerrainPass::compute_sum_reduction_prepass(CommandBuffer *command_buffer) {
        device->begin_debug_utils_label(command_buffer, "CBT Sum Reduction Prepass", nullptr);
        cbt_sum_reduction_prepass_program->bind(command_buffer);

        // Proper buffer access transition from vertex shader input to compute shader
        BufferBarrierInfo cbt_buffer_barrier_info[] = {
            {
                .buffer_id = cbt_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
            },
            {
                .buffer_id = cbt_leaf_count_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_WRITE,
            },
            {
                .buffer_id = cbt_draw_indirect_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                .src_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_WRITE,
            },
        };

        command_buffer->prepare_buffer(cbt_buffer_barrier_info, cast_u32(std::size(cbt_buffer_barrier_info)));

        ScopedGpuProfiling(command_buffer, "Sum Reduction Prepass");

        PushConstant push_constants = {
            .data = &cbt_depth,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(uint32_t),
            .offset = 0,
        };

        command_buffer->set_push_constants(cbt_sum_reduction_prepass_program->get_pipeline_id(), &push_constants, 1);
        uint32_t local_work_size = rendering_utils::get_workgroup_size((1 << cbt_depth) / 32, 256);
        command_buffer->dispatch(local_work_size, 1, 1);
        device->end_debug_utils_label(command_buffer);
    }

    void TerrainPass::compute_sum_reduction(CommandBuffer *command_buffer) {
        device->begin_debug_utils_label(command_buffer, "CBT Sum Reduction", nullptr);
        // cbt_depth = 8, so we start with 7 and
        uint32_t num_dispatches = cbt_depth - 6;
        cbt_sum_reduction_program->bind(command_buffer);

        // Proper buffer access transition from vertex shader input to compute shader
        BufferBarrierInfo cbt_buffer_barrier_info[] = {
            {
                .buffer_id = cbt_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
            },
        };

        ScopedGpuProfiling(command_buffer, "Sum Reduction");
        for (int level = num_dispatches; level >= 0; level--) {
            command_buffer->prepare_buffer(cbt_buffer_barrier_info, cast_u32(std::size(cbt_buffer_barrier_info)));
            PushConstant push_constants = {
                .data = &level,
                .shader_stage = SHADER_STAGE_COMPUTE,
                .size = sizeof(uint32_t),
                .offset = 0,
            };
            command_buffer->set_push_constants(cbt_sum_reduction_program->get_pipeline_id(), &push_constants, 1);
            uint32_t local_work_size = rendering_utils::get_workgroup_size(1 << level, 256);
            command_buffer->dispatch(local_work_size, 1, 1);
        }
        device->end_debug_utils_label(command_buffer);
    }

    void TerrainPass::update_subdivision(CommandBuffer *command_buffer, Camera *camera) {
        ScopedGpuProfiling(command_buffer, "Update Subdivision");
        device->begin_debug_utils_label(command_buffer, "CBT Update Subdivision", nullptr);

        struct PushConstantData {
            glm::mat4 VP;
            glm::vec4 frustum_planes[6];
            glm::vec4 subdivision_info;
        } push_constant_data;
        push_constant_data.VP = camera->get_view_projection_transform();

        Frustum &frustum = camera->get_frustum();
        for (int i = 0; i < 6; ++i) {
            push_constant_data.frustum_planes[i] = glm::vec4(frustum.planes[i].normal, frustum.planes[i].distance);
        }
        push_constant_data.subdivision_info = glm::vec4(subdivision_mode, lod_factor, 0.0f, 0.0f);

        PushConstant push_constant = {
            .data = &push_constant_data,
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(push_constant_data),
            .offset = 0,
        };

        cbt_subdivision_program->set_push_constant(&push_constant, 1);
        cbt_subdivision_program->bind(command_buffer);

        // Proper buffer access transition from vertex shader input to compute shader
        BufferBarrierInfo cbt_buffer_barrier_info[] = {
            {
                .buffer_id = cbt_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
            },
            {
                .buffer_id = cbt_leaf_count_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ,
            },
        };

        command_buffer->prepare_buffer(cbt_buffer_barrier_info, 2);
        uint32_t work_count = rendering_utils::get_workgroup_size(cbt_leaf_count_ptr[0], 256);
        command_buffer->dispatch(work_count, 1, 1);
        device->end_debug_utils_label(command_buffer);
    }

    void TerrainPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Scene *scene) {
        subdivision_mode = 1.0f - subdivision_mode;

        Camera *camera = scene->get_camera();

        uint32_t screenWidth, screenHeight;
        Window::get()->get_size(&screenWidth, &screenHeight);

        const uint32_t gpuSubdivision = 2;
        const float pixelLengthTarget = 3.0f;
        float tmp = 2.0f * tan(glm::radians(camera->get_fov()) / 2.0f) / screenHeight * (1 << gpuSubdivision) * pixelLengthTarget;
        lod_factor = -2.0f * std::log2(tmp) + 2.0f;
        TextRenderer *renderer = TextRenderManager::get()->get_default();
        renderer->AddText(std::to_string(cbt_leaf_count_ptr[0]), glm::vec2(20.0f, 600.0f));
    }

    void TerrainPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "TerrainPass", nullptr);

        update_subdivision(command_buffer, scene->get_camera());

        compute_sum_reduction_prepass(command_buffer);

        compute_sum_reduction(command_buffer);

        ScopedGpuProfiling(command_buffer, "Render Terrain");
        // Prepare cbt_buffer for vertex read
        BufferBarrierInfo barrier_infos[] = {
            {
                .buffer_id = cbt_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
                .dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ,
            },
            {
                .buffer_id = cbt_draw_indirect_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_WRITE,
                .dst_stage_mask = PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                .dst_access_mask = ACCESS_FLAG_INDIRECT_COMMAND_READ,
            },

        };

        command_buffer->prepare_buffer(barrier_infos, cast_u32(std::size(barrier_infos)));

        command_buffer->begin_render_pass(node, frame_graph);

        UniformSetID uniform_sets[] = {
            scene->per_frame_uniform_set,
            cbt_vert_set,
        };

        terrain_shader->set_uniform_sets(uniform_sets, cast_u32(std::size(uniform_sets)));
        terrain_shader->bind(command_buffer, &node->renderpass_info);

        // @TODO fix synchronization issues
        uint32_t instanceCount = cbt_leaf_count_ptr[0];
        command_buffer->draw_indirect(cbt_draw_indirect_buffer, 0, 1, sizeof(uint32_t) * 4);
        command_buffer->draw(3, instanceCount, 0, 0);
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    TerrainPass::~TerrainPass() {
        device->destroy_textures(&texture_heightmap, 1);
        BufferID buffers[] = {cbt_buffer, cbt_leaf_count_buffer, cbt_draw_indirect_buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
    }

} // namespace mirai