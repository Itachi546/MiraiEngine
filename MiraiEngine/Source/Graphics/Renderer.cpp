#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Math/MathUtils.hpp"
#include "PipelineLoader.hpp"
#include "Common/Random.hpp"
#include "RenderPass/TAAResolvePass.hpp"

#include <cstring>
#include <algorithm>
#include <execution>
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
        shader_hashmap = std::make_unique<ShaderHashMap>();
        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());

        current_frame_jitter = glm::vec2(0.0f);
        prev_frame_jitter = glm::vec2(0.0f);
        prev_frame_VP = glm::mat4(1.0f);
    }

    void Renderer::initialize() {
        // Per frame staging buffer
        uint32_t total_frames = device->get_swapchain_image_count();
        BufferDescription buffer_desc = {
            .size = cast_u32(total_frames * k_staging_buffer_size_per_frame),
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        Log::Info("Total Staging Buffer Memory: ", utils::bytes_to_mb(buffer_desc.size), " mb");
        Log::Info("Total Staging Buffer Memory/PerFrame: ", utils::bytes_to_mb(buffer_desc.size / total_frames), " mb");

        per_frame_staging_buffer = device->create_buffer(&buffer_desc, "per_frame_staging_buffer");
        per_frame_staging_buffer_ptr = device->map_buffer(per_frame_staging_buffer);

        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        buffer_desc.size = DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE;

        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT;
        BufferID geometry_buffer = device->create_buffer(&buffer_desc, "global_vertex_buffer");
        vertex_buffer_allocator.init(geometry_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE, 0);

        buffer_desc.usage_flags = BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT + BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT;
        BufferID index_buffer = device->create_buffer(&buffer_desc, "global_index_buffer");
        index_buffer_allocator.init(index_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE);

        buffer_desc.size = K_MAX_ENTITIES * sizeof(glm::mat4);
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
        global_transform_buffer = device->create_buffer(&buffer_desc, "global_transform_buffer");

        buffer_desc.size = K_MAX_ENTITIES * K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
        global_material_buffer = device->create_buffer(&buffer_desc, "global_material_buffer");

        prev_frame_VP = scene->get_camera()->get_view_projection_transform();
        prev_frame_jitter = current_frame_jitter = glm::vec2(0.0f);
    }

    void Renderer::on_load_resources() {
        if (pipeline_description_file.size() > 0)
            preload_shaders(shader_hashmap.get(), pipeline_description_file);

        // Trigger scene update so that transforms are updated
        // @TODO Fix this
        scene->update();
        prev_frame_VP = scene->get_camera()->get_view_projection_transform();

        // Create acceleration structure for scene
        auto &render_list = scene->render_object_list;

        auto &component_manager = scene->ecs->component_manager;
        std::vector<AccelerationStructureMeshInfo> mesh_infos(render_list.size());
        for (uint32_t i = 0; i < render_list.size(); ++i) {
            RenderableObjectData &object = render_list[i];

            mesh_infos[i].vertex_buffer = {
                .buffer = object.vertex_buffer.buffer,
                .offset = object.vertex_offset * object.vertex_stride,
                .count = object.index_count,
                .stride = object.vertex_stride,
            };

            mesh_infos[i].index_buffer = {
                .buffer = object.index_buffer.buffer,
                .offset = object.first_index * sizeof(uint32_t),
                .count = object.index_count,
                .stride = sizeof(uint32_t),
            };

            // @TODO a very long function :D
            TransformComponent *transform_component = component_manager->get_component<TransformComponent>(render_list[i].entity);
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
            AppSettings::enable_rt_shadow = true;

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        device->begin_debug_utils_label(command_buffer, "Copy global buffer", nullptr);

        // Update global transform buffer
        auto transform_components_ptr = component_manager->get_component_array<TransformComponent>();
        std::size_t transform_size_bytes = transform_components_ptr->components.size() * sizeof(glm::mat4);

        if (transform_size_bytes > 0) {
            uint32_t transform_buffer_offset = allocate_staging_buffer(cast_u32(transform_size_bytes), 1);
            uint8_t *transform_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + transform_buffer_offset);
            for (auto &component : transform_components_ptr->components) {
                std::memcpy(transform_array, &component.world_transform[0][0], sizeof(glm::mat4));
                transform_array += sizeof(glm::mat4);
            }

            BufferCopyRegion copy_region = {
                .src_offset = transform_buffer_offset,
                .dst_offset = 0,
                .size = transform_size_bytes,
            };
            command_buffer->copy_buffer(global_transform_buffer, per_frame_staging_buffer, &copy_region, 1);
        }

        // Update material buffer
        std::size_t material_size_bytes = scene->materials.size() * K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
        if (material_size_bytes > 0) {
            uint32_t material_buffer_offset = allocate_staging_buffer(cast_u32(material_size_bytes), 1);
            uint8_t *material_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + material_buffer_offset);

            uint8_t temp_buffer[K_MAX_MATERIAL_INSTANCE_DATA_SIZE];
            for (auto &mat : scene->materials) {
                std::memset(temp_buffer, 0, 64);
                uint32_t instance_data_size = mat->get_instance_data_size();
                ASSERT(instance_data_size <= K_MAX_MATERIAL_INSTANCE_DATA_SIZE);
                std::memcpy(temp_buffer, mat->get_instance_data(), mat->get_instance_data_size());

                std::memcpy(material_array, temp_buffer, 64);
                material_array += K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
            }

            BufferCopyRegion copy_region = {
                .src_offset = material_buffer_offset,
                .dst_offset = 0,
                .size = material_size_bytes,
            };
            command_buffer->copy_buffer(global_material_buffer, per_frame_staging_buffer, &copy_region, 1);
        }

        device->end_debug_utils_label(command_buffer);
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        frame_graph->compile(this);
    }

    void Renderer::copy_buffers() {
        // Reset staging buffer offset
        per_frame_staging_buffer_offset = 0;
        uint32_t current_frame = device->get_current_frame();

        // Copy per frame uniform data
        per_frame_uniform_buffer.offset = allocate_staging_buffer(sizeof(scene->per_frame_data), current_frame);
        per_frame_uniform_buffer.size = sizeof(Scene::FrameData);
        per_frame_uniform_buffer.buffer = per_frame_staging_buffer;

        uint8_t *staging_buffer_ptr = per_frame_staging_buffer_ptr + per_frame_uniform_buffer.offset;
        std::memcpy(staging_buffer_ptr, &scene->per_frame_data, sizeof(scene->per_frame_data));

        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        cascade_uniform_buffer.offset = allocate_staging_buffer(sizeof(cascade_info), current_frame);
        cascade_uniform_buffer.buffer = per_frame_staging_buffer;
        cascade_uniform_buffer.size = sizeof(cascade_info);
        staging_buffer_ptr = per_frame_staging_buffer_ptr + cascade_uniform_buffer.offset;

        // Copy cascade info
        std::memcpy(staging_buffer_ptr, &cascade_info, sizeof(cascade_info));
        // Calculate total memory required in staging buffer
        uint32_t total_entities = 0;
        uint32_t draw_indirect_size_bytes = 0;
        for (auto &batch : main_render_batches) {
            for (auto &mesh_batch : batch.meshes) {
                uint32_t num_entity = cast_u32(mesh_batch.mesh_draw_infos.size());
                total_entities += num_entity;
                draw_indirect_size_bytes += sizeof(DrawIndexedIndirectCommand) * num_entity;
            }
        }

        uint32_t draw_data_instance_size = sizeof(uint32_t) * 4;
        uint32_t draw_data_size_bytes = total_entities * draw_data_instance_size;
        uint32_t draw_data_buffer_offset = allocate_staging_buffer(draw_data_size_bytes, current_frame);
        uint8_t *draw_data_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + draw_data_buffer_offset);

        uint32_t draw_indirect_buffer_offset = allocate_staging_buffer(draw_indirect_size_bytes, current_frame);
        uint8_t *draw_indirect_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + draw_indirect_buffer_offset);

        auto &component_manager = scene->ecs->component_manager;
        for (auto &render_batch : main_render_batches) {
            for (auto &batch : render_batch.meshes) {
                uint32_t num_entity = cast_u32(batch.mesh_draw_infos.size());

                batch.draw_data_buffer_view.buffer = per_frame_staging_buffer;
                batch.draw_data_buffer_view.offset = draw_data_buffer_offset;
                batch.draw_data_buffer_view.size = num_entity * draw_data_instance_size;

                batch.draw_indirect_buffer_view.buffer = per_frame_staging_buffer;
                batch.draw_indirect_buffer_view.offset = draw_indirect_buffer_offset;
                batch.draw_indirect_buffer_view.size = sizeof(DrawIndexedIndirectCommand) * num_entity;

                for (uint32_t e = 0; e < num_entity; ++e) {
                    const MeshDrawInfo &draw_info = batch.mesh_draw_infos[e];
                    std::memcpy(draw_indirect_array, &draw_info.draw_info, sizeof(DrawIndexedIndirectCommand));
                    draw_indirect_array += sizeof(DrawIndexedIndirectCommand);

                    uint32_t draw_data[] = {
                        draw_info.transform_index,
                        draw_info.material_index,
                        0,
                        0,
                    };
                    std::memcpy(draw_data_array, &draw_data, draw_data_instance_size);
                    draw_data_array += draw_data_instance_size;
                }
                draw_indirect_buffer_offset += sizeof(DrawIndexedIndirectCommand) * num_entity;
                draw_data_buffer_offset += draw_data_instance_size * num_entity;
            }
        }
    }

    void Renderer::update_uniform_set(CommandBuffer *command_buffer) {
        // Initialize PerFrame uniform set
        UniformLayout layout = {
            .binding = 0,
            .binding_type = BINDING_TYPE_UNIFORM_BUFFER,
            .shader_stage = SHADER_STAGE_VERTEX | SHADER_STAGE_FRAGMENT,
        };
        per_frame_uniform_set = command_buffer->create_uniform_set(&layout, 1, 0);

        UniformBinding binding = {
            .resource_id = per_frame_uniform_buffer.buffer,
            .buffer_info = {
                .offset = per_frame_uniform_buffer.offset,
                .range = per_frame_uniform_buffer.size,
            },
        };
        device->update_uniform_set(per_frame_uniform_set, &binding, 1);

        layout.shader_stage = SHADER_STAGE_VERTEX;
        vt_per_frame_uniform_set = command_buffer->create_uniform_set(&layout, 1, 0);
        device->update_uniform_set(vt_per_frame_uniform_set, &binding, 1);

        UniformLayout transform_material_layouts[] = {
            {.binding = 0, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_VERTEX},
            {.binding = 1, .binding_type = BINDING_TYPE_STORAGE_BUFFER, .shader_stage = SHADER_STAGE_FRAGMENT},
        };
        transform_material_set = command_buffer->create_uniform_set(transform_material_layouts, cast_u32(std::size(transform_material_layouts)), 3);

        UniformBinding bindings[] = {
            {.resource_id = global_transform_buffer},
            {.resource_id = global_material_buffer},
        };
        device->update_uniform_set(transform_material_set, bindings, cast_u32(std::size(bindings)));

        transform_set = command_buffer->create_uniform_set(transform_material_layouts, 1, 3);
        device->update_uniform_set(transform_set, bindings, 1);
    }

    uint32_t Renderer::allocate_staging_buffer(uint32_t size, uint32_t current_frame, uint32_t alignment) {
        uint32_t per_frame_offset = current_frame * k_staging_buffer_size_per_frame;
        uint32_t next_ptr = per_frame_offset + per_frame_staging_buffer_offset;

        // Round to the multiple of 16
        size = align_memory(size, alignment);
        if (per_frame_staging_buffer_offset + size > k_staging_buffer_size_per_frame)
            Log::Fatal("Not enough per frame staging buffer ");

        per_frame_staging_buffer_offset += size;
        return next_ptr;
    }

    void Renderer::compile_passes() {
    }

    void Renderer::update() {
        // Update camera jitter
        FrameGraphNode *node = frame_graph->get_node("deferred_pass");
        Camera *camera = scene->get_camera();
        if (node) {
            ASSERT(node != nullptr);

            prev_frame_VP = camera->get_view_projection_transform();
            prev_frame_jitter = current_frame_jitter;

            glm::vec2 inv_resolution = 1.0f / glm::vec2{node->width, node->height};

            current_frame_jitter = halton23_sequence(jitter_index) * 2.0f - 1.0f;
            current_frame_jitter *= inv_resolution;
            jitter_index = (jitter_index + 1) % jitter_period;

            TAAResolvePass *taa = (TAAResolvePass *)frame_graph->get_renderer("taa_resolve_pass");
            if (taa && taa->enable_taa)
                camera->set_jitter_factor(current_frame_jitter);
            else
                camera->set_jitter_factor(glm::vec2(0.0f));
        }
        scene->update();

        main_render_batches.clear();

        Frustum &frustum = camera->get_frustum();
        DrawBatchGenerator::CreateBatch(scene.get(), &frustum, camera->position, main_render_batches, BATCH_FILTER_FLAG_ALPHA_MASK | BATCH_FILTER_FLAG_OPAQUE | BATCH_FILTER_FLAG_TRANSPARENT);

        std::for_each(std::execution::par_unseq, main_render_batches.begin(), main_render_batches.end(), [](RenderBatch &batch) {
            batch.sort();
        });

        frame_graph->update(this);

        FrameGraphNode *shadow_pass = frame_graph->get_node("directional_shadow_pass");
        FrameGraphNode *rt_shadow_pass = frame_graph->get_node("rt_directional_shadow_pass");
        if (rt_shadow_pass)
            rt_shadow_pass->enabled = AppSettings::enable_rt_shadow;
        if (shadow_pass)
            shadow_pass->enabled = !AppSettings::enable_rt_shadow;
    }

    void copy_continuous_region(CommandBuffer *cb, const std::vector<uint32_t> &indices, BufferView src, BufferView dst, uint32_t data_element_size) {
        BufferBarrierInfo barrier_infos[] = {
            {
                .buffer_id = dst.buffer,
                .offset = dst.offset,
                .size = dst.size,
                .src_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
                .src_access_mask = ACCESS_FLAG_SHADER_READ,
                .dst_stage_mask = PIPELINE_STAGE_COPY_BIT,
                .dst_access_mask = ACCESS_FLAG_TRANSFER_WRITE,
            },
        };

        cb->prepare_buffer(barrier_infos, cast_u32(std::size(barrier_infos)));

        uint32_t src_start_index = 0;
        uint32_t dst_start_index = indices[0];

        std::vector<BufferCopyRegion> copy_regions;
        for (uint32_t i = 1; i < indices.size(); ++i) {
            if (indices[i] == dst_start_index + 1)
                continue;

            uint32_t src_end_index = i;
            uint32_t range = src_end_index - src_start_index;

            copy_regions.push_back({
                .src_offset = src.offset + src_start_index * sizeof(glm::mat4),
                .dst_offset = dst.offset + dst_start_index * sizeof(glm::mat4),
                .size = range * data_element_size,
            });
            src_start_index = i;
            dst_start_index = indices[i];
        }

        uint32_t src_end_index = cast_u32(indices.size());
        uint32_t range = src_end_index - src_start_index;
        copy_regions.push_back({
            .src_offset = src.offset + src_start_index * sizeof(glm::mat4),
            .dst_offset = dst.offset + dst_start_index * sizeof(glm::mat4),
            .size = range * data_element_size,
        });
        cb->copy_buffer(dst.buffer, src.buffer, copy_regions.data(), cast_u32(copy_regions.size()));

        barrier_infos[0].src_access_mask = ACCESS_FLAG_TRANSFER_WRITE;
        barrier_infos[0].dst_access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_infos[0].src_stage_mask = PIPELINE_STAGE_COPY_BIT;
        barrier_infos[0].dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT;
        cb->prepare_buffer(barrier_infos, 1);
    }

    void Renderer::patch_global_data(CommandBuffer *command_buffer) {
        if (scene->updated_transforms.size() == 0 && scene->updated_materials.size() == 0)
            return;

        ScopedGpuProfiling(command_buffer, "Patch global buffers");
        uint32_t current_frame = device->get_current_frame();

        // Patch transforms
        auto &comp_manager = scene->ecs->component_manager;
        if (scene->updated_transforms.size() > 0) {
            auto &transform_components = comp_manager->get_component_array<TransformComponent>()->components;
            auto &updated_transforms = scene->updated_transforms;

            uint32_t transform_count = cast_u32(updated_transforms.size());
            uint32_t transform_size = transform_count * sizeof(glm::mat4);
            uint32_t transform_buffer_offset = allocate_staging_buffer(transform_size, current_frame);
            uint8_t *transform_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + transform_buffer_offset);
            for (uint32_t index : updated_transforms) {
                std::memcpy(transform_array, &transform_components[index].world_transform[0][0], sizeof(glm::mat4));
                transform_array += sizeof(glm::mat4);
            }

            copy_continuous_region(command_buffer, updated_transforms,
                                   BufferView{
                                       per_frame_staging_buffer,
                                       transform_buffer_offset,
                                       transform_size,
                                   },
                                   BufferView{
                                       global_transform_buffer,
                                       0,
                                       K_MAX_ENTITIES * sizeof(glm::mat4),
                                   },
                                   sizeof(glm::mat4));
        }

        // Patch Materials
        if (scene->updated_materials.size() > 0) {
            auto &materials = scene->materials;
            auto &updated_materials = scene->updated_materials;

            uint32_t material_count = cast_u32(updated_materials.size());
            uint32_t material_size = material_count * K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
            uint32_t material_buffer_offset = allocate_staging_buffer(material_size, current_frame);
            uint8_t *material_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + material_buffer_offset);

            uint8_t temp_buffer[K_MAX_MATERIAL_INSTANCE_DATA_SIZE];
            for (uint32_t index : updated_materials) {
                std::memset(temp_buffer, 0, 64);
                uint32_t instance_data_size = materials[index]->get_instance_data_size();
                ASSERT(instance_data_size <= K_MAX_MATERIAL_INSTANCE_DATA_SIZE);
                std::memcpy(temp_buffer, materials[index]->get_instance_data(), instance_data_size);
                std::memcpy(material_array, temp_buffer, 64);
                material_array += K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
            }

            copy_continuous_region(command_buffer, updated_materials,
                                   BufferView{
                                       per_frame_staging_buffer,
                                       material_buffer_offset,
                                       material_size,
                                   },
                                   BufferView{
                                       global_material_buffer,
                                       0,
                                       K_MAX_ENTITIES * K_MAX_MATERIAL_INSTANCE_DATA_SIZE,
                                   },
                                   K_MAX_MATERIAL_INSTANCE_DATA_SIZE);
        }
    }

    void Renderer::render() {

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();
        cb->begin();

        miProfiler::BeginFrame(cb);
        {
            ScopedGpuProfiling(cb, "Gpu Time");
            // Copy per frame data from staging buffer to gpu uniform buffer
            copy_buffers();

            // Patch transform and Materials if it has changed
            patch_global_data(cb);

            update_uniform_set(cb);

            frame_graph->render(cb, this);

            device->queue_command_buffer(cb);
        }
        miProfiler::EndFrame();

        device->present();
    }

    Renderer::~Renderer() {
        BufferID buffers[] = {per_frame_staging_buffer, vertex_buffer_allocator.buffer, index_buffer_allocator.buffer, global_material_buffer, global_transform_buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
        shader_hashmap->destroy();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai