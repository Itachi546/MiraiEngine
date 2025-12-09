#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Math/MathUtils.hpp"

#include <cstring>

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer() {
        ASSERT(Instance == nullptr);
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        texture_cache = std::make_unique<TextureCache>();
        miProfiler::Initialize();

        // Preload shaders
        shader_manager = std::make_unique<ShaderManager>();
        shader_manager->load("overlay_skybox", {"SPIRV/fullscreen.vert.spv", "SPIRV/skybox.frag.spv"}, {.depth_test = true});
        shader_manager->load("depth_prepass", {"SPIRV/depth-prepass.vert.spv"}, {.depth_test = true, .depth_write = true});
        shader_manager->load("pbr_forward", {"SPIRV/forward-pass.vert.spv", "SPIRV/forward-pass.frag.spv"}, {.depth_test = true, .depth_write = false});
        shader_manager->load("pbr_transparent", {"SPIRV/forward_pass.vert.spv", "SPIRV/transparent.frag.spv"}, {.cull_mode = CULL_MODE_NONE, .depth_test = true, .depth_write = true, .blend = true});
        shader_manager->load("gbuffer_pass", {"SPIRV/deferred.vert.spv", "SPIRV/deferred.frag.spv"}, {.depth_test = true, .depth_write = true});
        shader_manager->load("pbr_deferred", {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred_lighting.frag.spv"}, {.cull_mode = CULL_MODE_NONE});
        shader_manager->load("pbr_deferred_rt", {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred_lighting_rt.frag.spv"}, {.cull_mode = CULL_MODE_NONE});
        shader_manager->load("csm_shadow", {"SPIRV/cascaded_shadow.vert.spv"}, {.cull_mode = CULL_MODE_NONE, .depth_test = true, .depth_write = true, .depth_clamp = true});
        shader_manager->load("swapchain_copy_rgba", {"SPIRV/fullscreen.vert.spv", "SPIRV/swapchain-copy.frag.spv"}, {.cull_mode = CULL_MODE_BACK});
        shader_manager->load("text_render_2d", {"SPIRV/font.vert.spv", "SPIRV/font.frag.spv"}, {.blend = true});
        shader_manager->load("line_3d", {"SPIRV/line.vert.spv", "SPIRV/line.frag.spv"}, {.depth_test = true, .topology = TOPOLOGY_LINE_LIST});

        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());
    }

    void Renderer::initialize() {
        // this->scene->on_initialize();

        // Create acceleration structure for scene
        auto &render_list = scene->render_object_list;

        std::vector<AccelerationStructureMeshInfo> mesh_infos(render_list.size());
        for (uint32_t i = 0; i < render_list.size(); ++i) {
            RenderableObjectData &object = render_list[i];

            mesh_infos[i].vertex_buffer = {
                .buffer = object.vertex_buffer,
                .offset = cast_u32(object.vertex_offset * sizeof(Vertex)),
                .count = object.vertex_count,
                .stride = sizeof(Vertex),
            };

            mesh_infos[i].index_buffer = {
                .buffer = object.index_buffer,
                .offset = cast_u32(object.index_offset * sizeof(uint32_t)),
                .count = object.index_count,
                .stride = sizeof(uint32_t),
            };

            // @TODO a very long function :D
            TransformComponent *transform_component = scene->component_manager->get_component<TransformComponent>(render_list[i].entity);
            // The default representation of glm is column major while the VkTransformKHR uses row major
            // glm::mat4 transform = glm::transpose(transform_component.world_transform);
            glm::mat4 transform = transform_component->world_transform;
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 4; ++x) {
                    mesh_infos[i].transform[y][x] = transform[x][y];
                }
            }
        }
        if (mesh_infos.size() > 0)
            device->create_acceleration_structure(mesh_infos.data(), cast_u32(mesh_infos.size()));

        if (device->supports_raytracing())
            enable_rt_shadow = true;

        // Per frame staging buffer
        uint32_t total_frames = device->get_swapchain_image_count();
        BufferDescription buffer_desc = {
            .size = cast_u32(total_frames * k_staging_buffer_size_per_frame),
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        Log::Info("Total Staging Buffer Memory: ", utils::bytes_to_mb(buffer_desc.size), " mb");
        Log::Info("Total Staging Buffer Memory/PerFrame: ", utils::bytes_to_mb(buffer_desc.size / total_frames), " mb");

        per_frame_staging_buffer = device->create_buffer(&buffer_desc, "per_frame_staging_buffer");
        per_frame_staging_buffer_ptr = device->map_buffer(per_frame_staging_buffer);

        buffer_desc.size = k_transform_buffer_size;
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        // Allocate transform buffer
        transform_buffer = device->create_buffer(&buffer_desc, "transform_buffer");
        Log::Info("Total Transform Buffer Memory: ", utils::bytes_to_mb(k_transform_buffer_size), " mb");

        // Allocate material buffer
        buffer_desc.size = k_material_buffer_size;
        material_buffer = device->create_buffer(&buffer_desc, "material_buffer");
        Log::Info("Total Material Buffer Memory: ", utils::bytes_to_mb(k_material_buffer_size), " mb");

        // Initialize PerFrame Resources
        buffer_desc.size = sizeof(Scene::FrameData);
        buffer_desc.usage_flags = BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        per_frame_uniform_buffer = device->create_buffer(&buffer_desc, "per_frame_data_buffer");

        // Initialize cascade info
        buffer_desc.size = sizeof(DirectionalLightCascadeInfo);
        cascade_uniform_buffer = device->create_buffer(&buffer_desc, "cascade_uniform_buffer");

        // Initialize global goemetry buffer allocator
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        buffer_desc.size = DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE;
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT;

        BufferID geometry_buffer = device->create_buffer(&buffer_desc, "global_vertex_buffer");
        vertex_buffer_allocator.init(geometry_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE, 0);

        buffer_desc.usage_flags = BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
        BufferID index_buffer = device->create_buffer(&buffer_desc, "global_index_buffer");
        index_buffer_allocator.init(index_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE);

        // Initialize cascade uniform set
        if (scene->directional_light_info.enable_shadow) {
            UniformLayout cascade_buffer_layout = {
                .binding = 0,
                .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
                .shader_stage = SHADER_STAGE_VERTEX,
            };
            cascade_uniform_set = device->create_uniform_set(&cascade_buffer_layout, 1, 0, "cascade_uniform_set");
            UniformBinding cascade_uniform_binding = {.resource_id = cascade_uniform_buffer};
            device->update_uniform_set(cascade_uniform_set, &cascade_uniform_binding, 1);
        }
    }

    void Renderer::copy_buffers(CommandBuffer *cb) {
        ScopedCpuProfiling("Renderer::Copy Buffers");
        // Reset staging buffer offset
        per_frame_staging_buffer_offset = 0;

        std::vector<BufferBarrierInfo> barrier_infos(4);
        for (uint32_t i = 0; i < barrier_infos.size(); ++i) {
            barrier_infos[i].offset = 0;
            barrier_infos[i].size = UINT64_MAX;
            barrier_infos[i].src_stage_mask = PIPELINE_STAGE_ALL_COMMANDS_BIT;
            barrier_infos[i].src_access_mask = ACCESS_FLAG_SHADER_READ;
            barrier_infos[i].dst_stage_mask = PIPELINE_STAGE_TRANSFER_BIT;
            barrier_infos[i].dst_access_mask = ACCESS_FLAG_TRANSFER_WRITE;
        }
        barrier_infos[0].buffer_id = per_frame_uniform_buffer;
        barrier_infos[1].buffer_id = cascade_uniform_buffer;
        barrier_infos[2].buffer_id = transform_buffer;
        barrier_infos[3].buffer_id = material_buffer;

        // Prepare the buffer for copy
        cb->prepare_buffer(barrier_infos.data(), cast_u32(barrier_infos.size()));

        // Copy per frame uniform data
        uint32_t current_frame = device->get_current_frame();
        uint32_t staging_buffer_offset = allocate_staging_buffer(sizeof(scene->per_frame_data), current_frame);
        uint8_t *staging_buffer_ptr = per_frame_staging_buffer_ptr + staging_buffer_offset;

        std::memcpy(staging_buffer_ptr, &scene->per_frame_data, sizeof(scene->per_frame_data));
        cb->copy_buffer(per_frame_uniform_buffer, per_frame_staging_buffer, {
                                                                                .src_offset = staging_buffer_offset,
                                                                                .dst_offset = 0,
                                                                                .size = sizeof(scene->per_frame_data),
                                                                            });

        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        staging_buffer_offset = allocate_staging_buffer(sizeof(cascade_info), current_frame);
        staging_buffer_ptr = per_frame_staging_buffer_ptr + staging_buffer_offset;

        // Copy cascade info
        std::memcpy(staging_buffer_ptr, &cascade_info, sizeof(cascade_info));
        cb->copy_buffer(cascade_uniform_buffer, per_frame_staging_buffer, {
                                                                              .src_offset = staging_buffer_offset,
                                                                              .dst_offset = 0,
                                                                              .size = sizeof(cascade_info),
                                                                          });

        // Calculate total memory required in staging buffer
        uint32_t total_entities = 0;
        for (auto &batch : scene->main_render_batches) {
            total_entities += cast_u32(batch.transform_indices.size());
        }
        // @TODO do it in parallel
        // Allocate memory in staging buffer
        uint32_t transform_size_bytes = total_entities * sizeof(glm::mat4);
        uint32_t transform_buffer_offset = allocate_staging_buffer(transform_size_bytes, current_frame);
        glm::mat4 *transform_array = reinterpret_cast<glm::mat4 *>(per_frame_staging_buffer_ptr + transform_buffer_offset);

        uint32_t material_size_bytes = total_entities * sizeof(StandardPBRMaterial::PBRProperties);
        uint32_t material_buffer_offset = allocate_staging_buffer(material_size_bytes, current_frame);
        uint8_t *material_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + material_buffer_offset);
        // Copy the transform/material data in staging buffer
        uint32_t offset = 0;
        for (auto &batch : scene->main_render_batches) {
            uint32_t num_entity = cast_u32(batch.transform_indices.size());

            batch.transform_buffer_view.buffer = transform_buffer;
            batch.transform_buffer_view.offset = offset * sizeof(glm::mat4);
            batch.transform_buffer_view.size = sizeof(glm::mat4) * num_entity;

            uint32_t material_batch_size = sizeof(StandardPBRMaterial::PBRProperties) * num_entity;
            batch.material_buffer_view.buffer = material_buffer;
            batch.material_buffer_view.offset = offset * sizeof(StandardPBRMaterial::PBRProperties);
            batch.material_buffer_view.size = material_batch_size;

            for (uint32_t e = 0; e < num_entity; ++e) {
                TransformComponent &component = scene->component_manager->get_component_array<TransformComponent>()->components[batch.transform_indices[e]];
                transform_array[offset + e] = component.world_transform;

                auto &material = scene->materials[batch.material_indices[e]];
                uint32_t instance_data_size = material->get_instance_data_size();
                std::memcpy(material_array, material->get_instance_data(), instance_data_size);
                material_array += instance_data_size;
            }
            offset += num_entity;
        }
        if (transform_size_bytes > 0) {
            cb->copy_buffer(transform_buffer, per_frame_staging_buffer, {
                                                                            .src_offset = transform_buffer_offset,
                                                                            .dst_offset = 0,
                                                                            .size = transform_size_bytes,
                                                                        });
        }

        if (material_size_bytes > 0) {
            cb->copy_buffer(material_buffer, per_frame_staging_buffer, {
                                                                           .src_offset = material_buffer_offset,
                                                                           .dst_offset = 0,
                                                                           .size = material_size_bytes,
                                                                       });
        }

        // Prepare the buffer for shader read
        for (uint32_t i = 0; i < barrier_infos.size(); ++i) {
            barrier_infos[i].src_stage_mask = PIPELINE_STAGE_TRANSFER_BIT;
            barrier_infos[i].src_access_mask = ACCESS_FLAG_TRANSFER_WRITE;
            barrier_infos[i].dst_stage_mask = PIPELINE_STAGE_ALL_COMMANDS_BIT;
            barrier_infos[i].dst_access_mask = ACCESS_FLAG_SHADER_READ;
        }

        cb->prepare_buffer(barrier_infos.data(), cast_u32(barrier_infos.size()));
    }

    uint32_t Renderer::allocate_staging_buffer(uint32_t size, uint32_t current_frame) {
        ASSERT_MSG(per_frame_staging_buffer_offset + size <= k_staging_buffer_size_per_frame, "Staging buffer size is not enough");
        uint32_t per_frame_offset = current_frame * k_staging_buffer_size_per_frame;
        uint32_t next_ptr = per_frame_offset + per_frame_staging_buffer_offset;

        // Round to the multiple of 16
        size = (size + 16 - 1) & ~15;
        per_frame_staging_buffer_offset += size;
        return next_ptr;
    }

    void Renderer::compile_passes() {
    }

    void Renderer::update() {
        scene->update();
        frame_graph->update(this);

        FrameGraphNode *shadow_pass = frame_graph->get_node("directional_shadow_pass");
        FrameGraphNode *rt_shadow_pass = frame_graph->get_node("rt_directional_shadow_pass");
        if (rt_shadow_pass)
            rt_shadow_pass->enabled = enable_rt_shadow;
        if (shadow_pass)
            shadow_pass->enabled = !enable_rt_shadow;
    }

    void Renderer::render() {

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();
        cb->begin();

        miProfiler::BeginFrame(cb);
        {
            ScopedGpuProfiling(cb, "Gpu Time");
            // Copy per frame data from staging buffer to gpu uniform buffer
            copy_buffers(cb);

            frame_graph->render(cb, this);

            device->queue_command_buffer(cb);
        }
        miProfiler::EndFrame();

        device->present();
    }

    Renderer::~Renderer() {
        BufferID buffers[] = {per_frame_staging_buffer, transform_buffer, material_buffer, cascade_uniform_buffer, per_frame_uniform_buffer, vertex_buffer_allocator.buffer, index_buffer_allocator.buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
        shader_manager.reset();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai