#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderHashMap.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Math/MathUtils.hpp"
#include "PipelineLoader.hpp"
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
        shader_hashmap = std::make_unique<ShaderHashMap>();
        preload_shaders(shader_hashmap.get());
        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());
    }

    void Renderer::initialize() {
        // Create acceleration structure for scene
        auto &render_list = scene->render_object_list;

        std::vector<AccelerationStructureMeshInfo> mesh_infos(render_list.size());
        for (uint32_t i = 0; i < render_list.size(); ++i) {
            RenderableObjectData &object = render_list[i];

            mesh_infos[i].vertex_buffer = {
                .buffer = object.vertex_buffer.buffer,
                .offset = cast_u32(object.vertex_offset * sizeof(Vertex)),
                .count = object.vertex_count,
                .stride = sizeof(Vertex),
            };

            mesh_infos[i].index_buffer = {
                .buffer = object.index_buffer.buffer,
                .offset = cast_u32(object.index_offset * sizeof(uint32_t)),
                .count = object.index_count,
                .stride = sizeof(uint32_t),
            };

            // @TODO a very long function :D
            auto &component_manager = scene->ecs->component_manager;
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
            enable_rt_shadow = true;

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
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_INDEX_BUFFER_BIT;

        BufferID geometry_buffer = device->create_buffer(&buffer_desc, "global_vertex_buffer");
        vertex_buffer_allocator.init(geometry_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE, 0);

        buffer_desc.usage_flags = BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;
        BufferID index_buffer = device->create_buffer(&buffer_desc, "global_index_buffer");
        index_buffer_allocator.init(index_buffer, DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE);
    }

    void Renderer::copy_buffers() {
        // @TODO this is not correct and shouldn't be done
        ScopedCpuProfiling("Renderer::Copy Buffers");
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
        uint32_t material_size_bytes = 0;
        uint32_t draw_indirect_size_bytes = 0;
        for (auto &batch : scene->main_render_batches) {
            for (auto &mesh_batch : batch.meshes) {
                uint32_t num_entity = cast_u32(mesh_batch.mesh_draw_infos.size());
                total_entities += num_entity;

                uint32_t material_index = mesh_batch.mesh_draw_infos[0].material_index;
                material_size_bytes += scene->materials[material_index]->get_instance_data_size() * num_entity;
                draw_indirect_size_bytes += sizeof(DrawIndexedIndirectCommand) * num_entity;
            }
        }

        // Allocate memory in staging buffer
        uint32_t transform_size_bytes = total_entities * sizeof(glm::mat4);
        uint32_t transform_buffer_offset = allocate_staging_buffer(transform_size_bytes, current_frame);
        uint8_t *transform_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + transform_buffer_offset);

        uint32_t material_buffer_offset = allocate_staging_buffer(material_size_bytes, current_frame);
        uint8_t *material_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + material_buffer_offset);

        uint32_t draw_indirect_buffer_offset = allocate_staging_buffer(draw_indirect_size_bytes, current_frame);
        uint8_t *draw_indirect_array = reinterpret_cast<uint8_t *>(per_frame_staging_buffer_ptr + draw_indirect_buffer_offset);

        // Copy the transform/material data in staging buffer
        uint32_t transform_offset_bytes = transform_buffer_offset;
        uint32_t material_offset_bytes = material_buffer_offset;
        auto &component_manager = scene->ecs->component_manager;

        for (auto &render_batch : scene->main_render_batches) {
            for (auto &batch : render_batch.meshes) {
                uint32_t num_entity = cast_u32(batch.mesh_draw_infos.size());
                batch.transform_buffer_view.buffer = per_frame_staging_buffer;
                batch.transform_buffer_view.offset = transform_offset_bytes;
                batch.transform_buffer_view.size = sizeof(glm::mat4) * num_entity;

                uint32_t material_index = batch.mesh_draw_infos[0].material_index;
                uint32_t instance_data_size = scene->materials[material_index]->get_instance_data_size();
                batch.material_buffer_view.buffer = per_frame_staging_buffer;
                batch.material_buffer_view.offset = material_offset_bytes;
                batch.material_buffer_view.size = num_entity * instance_data_size;

                batch.draw_indirect_buffer_view.buffer = per_frame_staging_buffer;
                batch.draw_indirect_buffer_view.offset = draw_indirect_buffer_offset;
                batch.draw_indirect_buffer_view.size = sizeof(DrawIndexedIndirectCommand) * num_entity;

                for (uint32_t e = 0; e < num_entity; ++e) {
                    const MeshDrawInfo &draw_info = batch.mesh_draw_infos[e];
                    TransformComponent &component = component_manager->get_component_array<TransformComponent>()->components[draw_info.transform_index];
                    std::memcpy(transform_array, &component.world_transform[0][0], sizeof(glm::mat4));

                    auto &material = scene->materials[draw_info.material_index];
                    std::memcpy(material_array, material->get_instance_data(), instance_data_size);
                    material_array += instance_data_size;
                    transform_array += sizeof(glm::mat4);

                    std::memcpy(draw_indirect_array, &draw_info.draw_info, sizeof(DrawIndexedIndirectCommand));
                    draw_indirect_array += sizeof(DrawIndexedIndirectCommand);
                }
                transform_offset_bytes += num_entity * sizeof(glm::mat4);
                material_offset_bytes += num_entity * instance_data_size;
                draw_indirect_buffer_offset += sizeof(DrawIndexedIndirectCommand) * num_entity;
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
    }

    uint32_t Renderer::allocate_staging_buffer(uint32_t size, uint32_t current_frame) {
        uint32_t per_frame_offset = current_frame * k_staging_buffer_size_per_frame;
        uint32_t next_ptr = per_frame_offset + per_frame_staging_buffer_offset;

        // Round to the multiple of 16
        size = (size + 16 - 1) & ~15;
        if (per_frame_staging_buffer_offset + size > k_staging_buffer_size_per_frame)
            Log::Fatal("Not enough per frame staging buffer ");

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
            copy_buffers();

            update_uniform_set(cb);

            frame_graph->render(cb, this);

            device->queue_command_buffer(cb);
        }
        miProfiler::EndFrame();

        device->present();
    }

    Renderer::~Renderer() {
        BufferID buffers[] = {per_frame_staging_buffer, vertex_buffer_allocator.buffer, index_buffer_allocator.buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
        shader_hashmap->destroy();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai