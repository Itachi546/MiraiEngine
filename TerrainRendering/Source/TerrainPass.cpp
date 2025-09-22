#include "TerrainPass.hpp"
#include "Engine/Profiler.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"

#define CBT_IMPLEMENTATION
#include "CBT.hpp"

namespace mirai {
    TerrainPass::TerrainPass(uint32_t width, uint32_t height, uint32_t cbt_depth) : FrameGraphRenderer("terrain_pass"),
                                                                                    width(width), height(height), cbt_depth(cbt_depth) {
        device = RenderingDevice::get();
    }

    void TerrainPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node) {
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

        UniformBinding bindings[] = {{cbt_buffer}, {cbt_leaf_count_buffer}};
        UniformLayout layouts[] = {
            {0, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_VERTEX},
            {1, BINDING_TYPE_STORAGE_BUFFER, SHADER_STAGE_COMPUTE},
        };

        cbt_buffer_vert_set = device->create_uniform_set(&layouts[0], 1, 1, "cbt_buffer_vert_set");
        device->update_uniform_set(cbt_buffer_vert_set, &bindings[0], 1);

        layouts[0].shader_stage = SHADER_STAGE_COMPUTE;
        cbt_buffer_comp_set = device->create_uniform_set(layouts, std::size(layouts), 0, "cbt_buffer_comp_set");
        device->update_uniform_set(cbt_buffer_comp_set, bindings, cast_u32(std::size(bindings)));

        cbt_init_program = std::make_unique<ComputeShader>("cbt_initialize");
        cbt_init_program->create_from_file("SPIRV/cbt_initialize.comp.spv");
        cbt_init_program->set_uniform_sets(&cbt_buffer_comp_set, 1);

        cbt_sum_reduction_program = std::make_unique<ComputeShader>("cbt_sumreduction");
        cbt_sum_reduction_program->create_from_file("SPIRV/cbt_sum_reduction.comp.spv");
        cbt_sum_reduction_program->set_uniform_sets(&cbt_buffer_comp_set, 1);

        cbt_subdivision_program = std::make_unique<ComputeShader>("cbt_subdivision");
        cbt_subdivision_program->create_from_file("SPIRV/cbt_subdivision.comp.spv");
        cbt_subdivision_program->set_uniform_sets(&cbt_buffer_comp_set, 1);

        // Initialize Terrain Shader
        terrain_shader = std::make_unique<ShaderMaterial>("Terrain Shader");
        terrain_shader->create_from_file({"SPIRV/terrain.vert.spv", "SPIRV/terrain.frag.spv"}, {
                                                                                                   .cull_mode = CULL_MODE_NONE,
                                                                                                   .depth_test = true,
                                                                                                   .depth_write = true,
                                                                                                   .polygon_mode = POLYGON_MODE_LINE,
                                                                                               });

        // Initialize CBT Buffer
        init_at_depth(13);
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

    void TerrainPass::compute_sum_reduction(CommandBuffer *command_buffer) {
        device->begin_debug_utils_label(command_buffer, "CBT Sum Reduction", nullptr);
        uint32_t num_dispatches = cbt_depth - 1;
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
            {
                .buffer_id = cbt_leaf_count_buffer,
                .offset = 0,
                .size = UINT64_MAX,
                .src_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_WRITE,
            },
        };

        command_buffer->prepare_buffer(cbt_buffer_barrier_info, 2);

        // For subsequent pass, it will be compute to compute dependency
        cbt_buffer_barrier_info[0].src_access_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        cbt_buffer_barrier_info[0].src_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE;

        for (int level = num_dispatches; level >= 0; level--) {
            PushConstant push_constants = {
                .data = &level,
                .shader_stage = SHADER_STAGE_COMPUTE,
                .size = sizeof(uint32_t),
                .offset = 0,
            };
            command_buffer->set_push_constants(cbt_sum_reduction_program->get_pipeline_id(), &push_constants, 1);
            uint32_t local_work_size = rendering_utils::get_workgroup_size(1 << level, 256);
            command_buffer->dispatch(local_work_size, 1, 1);

            // At last level we prepare the cbt_buffer for vertex shader
            if (level > 0) {
                command_buffer->prepare_buffer(cbt_buffer_barrier_info, 1);
            }
        }
        device->end_debug_utils_label(command_buffer);
    }

    void TerrainPass::update_subdivision(CommandBuffer *command_buffer, Camera *camera) {
        device->begin_debug_utils_label(command_buffer, "CBT Update Subdivision", nullptr);

        std::vector<glm::vec4> push_constant_data;
        Frustum &frustum = camera->get_frustum();
        for (int i = 0; i < 6; ++i) {
            push_constant_data.push_back(glm::vec4(frustum.planes[i].normal, frustum.planes[i].distance));
        }
        push_constant_data.push_back(glm::vec4(camera->position, subdivision_mode));

        PushConstant push_constant = {
            .data = push_constant_data.data(),
            .shader_stage = SHADER_STAGE_COMPUTE,
            .size = sizeof(float) * 4 * cast_u32(push_constant_data.size()),
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
    }

    void TerrainPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
        device->begin_debug_utils_label(command_buffer, "TerrainPass", nullptr);

        update_subdivision(command_buffer, scene->get_camera());

        compute_sum_reduction(command_buffer);

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
        };

        command_buffer->prepare_buffer(barrier_infos, cast_u32(std::size(barrier_infos)));

        command_buffer->begin_render_pass(node, frame_graph);

        UniformSetID uniform_sets[] = {
            scene->per_frame_uniform_set,
            cbt_buffer_vert_set,
        };

        terrain_shader->set_uniform_sets(uniform_sets, cast_u32(std::size(uniform_sets)));
        terrain_shader->bind(command_buffer, &node->renderpass_info);
        // @TODO may cause synchronization issue
        uint32_t instanceCount = cbt_leaf_count_ptr[0];
        command_buffer->draw(3, instanceCount, 0, 0);
        command_buffer->end_render_pass();

        device->end_debug_utils_label(command_buffer);
    }

    TerrainPass::~TerrainPass() {
        BufferID buffers[] = {cbt_buffer, cbt_leaf_count_buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
    }

} // namespace mirai