#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/ShadowSystem.hpp"
#include "RenderPass/RenderPassData.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/MeshData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Engine/Engine.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"
#include "Math/MathUtils.hpp"
#include "Math/Frustum.hpp"
#include "PipelineLoader.hpp"
#include "Common/JobSystem.hpp"
#include "RenderPass/TAAResolvePass.hpp"
#include "LineRenderer.hpp"

#include <cstring>
#include <algorithm>
#include <execution>
namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    struct GPULightData {
        glm::vec3 position;
        uint32_t flag;

        glm::vec3 direction;
        float intensity;

        uint32_t color;
        float radius_or_height;
        float inner_angle;
        float outer_angle;
    };
    static_assert(sizeof(GPULightData) % 16 == 0);

    Renderer::Renderer() {
        ASSERT(Instance == nullptr);
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        frame_flight_index = device->get_current_frame_in_flight_index();
        frame_id = 0;

        scene = std::make_unique<Scene>("default");
        texture_cache = std::make_unique<TextureCache>();
        line_renderer = std::make_unique<LineRenderer>();
        miProfiler::Initialize();

        // Preload shaders
        shader_registry = std::make_unique<ShaderRegistry>();
        preload_shaders();

        frame_graph = std::make_unique<FrameGraph>();
        frame_graph_blackboard = std::make_unique<FrameGraphBlackBoard>();
        // Add debug data to blackboard
        frame_graph_blackboard->add<RenderDebugData>(RenderDebugData{
            .split_percentage = 0.0f,
            .debug_param_index = 0,
            .show_debug_cascade_color = false,
            .enable_gamma_correction = true,
            .exposure = 1.0f,
            .mip_lod_bias = -0.5f,
            .bloom_strength = 0.04f,
            .bloom_radius = 1.0f,
        });

        shadow_system = std::make_unique<ShadowSystem>();
    }

    void Renderer::initialize() {
        geometry_buffer_allocator = std::make_unique<GPUPagedAllocator>();
        geometry_buffer_allocator->init();

        // Per frame staging buffer
        uint32_t total_frames = device->get_swapchain_image_count();

        BufferDescription buffer_desc = {
            .size = k_staging_buffer_size_per_frame,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };

        Log::Info("Total Staging Buffer Memory: ", utils::bytes_to_mb(buffer_desc.size) * total_frames, " mb");
        Log::Info("Total Staging Buffer Memory/PerFrame: ", utils::bytes_to_mb(buffer_desc.size), " mb");

        for (uint32_t i = 0; i < total_frames; ++i) {
            per_frame_allocator[i].init(buffer_desc, "PerFrameAllocator" + std::to_string(i));
        }

        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        buffer_desc.usage_flags = BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

        uint32_t transform_buffer_size = cast_u32(1024 * sizeof(glm::mat4));
        buffer_desc.size = transform_buffer_size;
        transform_buffer = BufferView{device->create_buffer(&buffer_desc, "TransformBuffer"), 0, transform_buffer_size};

        // Allocate material buffer
        uint32_t material_buffer_size = cast_u32(1024 * sizeof(Material3D));
        buffer_desc.size = material_buffer_size;
        material_buffer = BufferView{device->create_buffer(&buffer_desc, "MaterialBuffer"), 0, material_buffer_size};

        // Allocate light buffer
        uint32_t light_buffer_size = cast_u32(1024 * sizeof(GPULightData));
        buffer_desc.size = light_buffer_size;
        light_buffer = BufferView{device->create_buffer(&buffer_desc, "LightBuffer"), 0, light_buffer_size};

        // Allocate descriptor heap buffer
        buffer_desc.usage_flags = BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_CPU;

        uint32_t resource_descriptor_count = AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT + AppSettings::K_MAX_FRAME_IN_FLIGHTS * AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT;
        buffer_desc.size = device->calculate_resource_descriptors_size(resource_descriptor_count);
        resource_heap.buffer = device->create_buffer(&buffer_desc, "global_resource_heap");
        resource_heap.ptr = device->map_buffer(resource_heap.buffer);
        resource_heap.descriptor_size = device->get_resource_descriptor_size();
        resource_heap.size = buffer_desc.size;
        resource_heap.new_frame(frame_flight_index);
        // We allocate first n location for bindless texture, so that
        // we don't have to deal with conversion of textureID to descriptorIndex
        resource_heap.allocate(AppSettings::K_MAX_BINDLESS_TEXTURE_COUNT);

        // Allocate sampler heap
        buffer_desc.size = device->calculate_sampler_descriptors_size(AppSettings::K_SAMPLER_DESCRIPTOR_LIMIT);
        sampler_heap.buffer = device->create_buffer(&buffer_desc, "global_sampler_heap");
        sampler_heap.ptr = device->map_buffer(sampler_heap.buffer);
        sampler_heap.descriptor_size = device->get_sampler_descriptor_size();
        sampler_heap.size = buffer_desc.size;

        // Upload default sampler
        rendering_utils::upload_default_samplers(device.get(), sampler_heap.ptr);

        // Create skinning compute shader
        skinning_shader = std::make_unique<ComputeShader>("SkinningCS", "SPIRV/skinning.comp.spv");
        patch_buffer_shader = std::make_unique<ComputeShader>("PatchBufferCS", "SPIRV/patch-buffer.comp.spv");

        // Upload default texture
        blue_noise_texture128 = rendering_utils::load_texture2d_from_path("Assets/Textures/blue-noise-128.png");
        texture_cache->add_texture("noise-texture-128", blue_noise_texture128);
        add_bindless_texture(blue_noise_texture128);

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();

        initialize_scene_default_meshes(command_buffer);

        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();
    }

    void Renderer::on_load_resources() {

        Camera *camera = scene->get_camera();
        freezed_inv_VP = camera->get_inv_view_projection_transform();
        freezed_frustum_planes = camera->get_frustum_planes();

        frame_graph->compile();

        create_blas();
    }
    // @TODO do it in batch, so that we can wait for task at once in the end
    void Renderer::upload_transforms(CommandBuffer *command_buffer) {
        if (scene->updated_transforms.size() == 0 && frame_id > 0)
            return;

        auto &component_manager = scene->ecs->component_manager;
        auto mesh_comp_ptr = component_manager->get_component_array<MeshComponent>();

        uint32_t total_meshes = cast_u32(mesh_comp_ptr->components.size());

        uint64_t transform_data_size = total_meshes * sizeof(glm::mat4);

        bool should_reupload_data = false;
        if (transform_data_size > transform_buffer.size) {
            Log::Info("Resizing transform buffer: ", utils::bytes_to_mb(transform_data_size), "MB");
            BufferDescription buffer_desc = {
                .size = std::max(transform_data_size, transform_buffer.size * 2),
                .usage_flags = BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
            };
            device->resize_buffer(&buffer_desc, transform_buffer.buffer, false, "TransformBuffer");

            transform_buffer.offset = 0;
            transform_buffer.size = buffer_desc.size;
            should_reupload_data = true;
        }

        // If more than half of the transforms has changed or it is first frame, then we copy everything
        if (scene->updated_transforms.size() >= total_meshes / 2 || frame_id == 0 || should_reupload_data) {
            uint32_t total_transform_size_bytes = cast_u32(total_meshes * sizeof(glm::mat4));
            BufferView transform_staging_buffer = per_frame_allocator[frame_flight_index].allocate(total_transform_size_bytes);
            glm::mat4 *transform_array = reinterpret_cast<glm::mat4 *>(transform_staging_buffer.ptr);

            jobsystem::Dispatch(total_meshes, 64, [&](jobsystem::JobDispatchArg arg) {
                Entity entity = mesh_comp_ptr->entities[arg.job_index];
                MeshComponent *mesh_component = mesh_comp_ptr->get_component(entity);
                TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
                transform_array[mesh_component->gpu_index] = transform->world_transform;
            });
            jobsystem::Wait();

            BufferBarrierInfo barrier = {transform_buffer.buffer, transform_buffer.offset, transform_buffer.size, PIPELINE_STAGE_TRANSFER_BIT, ACCESS_FLAG_TRANSFER_WRITE};
            command_buffer->prepare_buffer(&barrier, 1);

            BufferCopyRegion copy_region = {
                .src_offset = transform_staging_buffer.offset,
                .dst_offset = 0,
                .size = total_transform_size_bytes,
            };
            command_buffer->copy_buffer(transform_buffer.buffer, transform_staging_buffer.buffer, &copy_region, 1);

        } else {
            uint32_t total_updated_transforms = cast_u32(scene->updated_transforms.size());
            uint32_t element_size = cast_u32(sizeof(glm::mat4));
            ASSERT(element_size % 4 == 0);

            uint32_t allocation_size = total_updated_transforms * element_size;
            BufferView src_buffer = per_frame_allocator[frame_flight_index].allocate(allocation_size);
            glm::mat4 *transform_array = reinterpret_cast<glm::mat4 *>(src_buffer.ptr);

            uint32_t patch_buffer_allocation_size = cast_u32(sizeof(BufferPatch) * total_updated_transforms);
            BufferView patch_buffer = per_frame_allocator[frame_flight_index].allocate(patch_buffer_allocation_size);
            BufferPatch *patch_array = reinterpret_cast<BufferPatch *>(patch_buffer.ptr);

            auto transform_component_array = component_manager->get_component_array<TransformComponent>();

            element_size /= 4;

            for (uint32_t i = 0; i < total_updated_transforms; ++i) {
                const auto [transform_index, gpu_index] = scene->updated_transforms[i];
                transform_array[i] = transform_component_array->components[transform_index].world_transform;

                uint32_t offset = gpu_index * element_size;
                patch_array[i] = BufferPatch{offset, element_size};
            }
            dispatch_patch_copy(command_buffer, patch_buffer, src_buffer, transform_buffer, total_updated_transforms);
        }

        scene->updated_transforms.clear();
    }

    void Renderer::upload_materials(CommandBuffer *command_buffer) {
        if (scene->updated_materials.size() == 0 && frame_id > 0)
            return;

        uint32_t total_materials = cast_u32(scene->materials.size());
        uint64_t material_data_size = sizeof(Material3D::Properties) * total_materials;

        bool should_reupload_data = false;
        if (material_data_size > material_buffer.size) {
            Log::Info("Resizing material buffer: ", utils::bytes_to_mb(material_data_size), "MB");
            BufferDescription buffer_desc = {
                .size = std::max(material_data_size, material_buffer.size * 2),
                .usage_flags = BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
            };

            device->resize_buffer(&buffer_desc, material_buffer.buffer, false, "MaterialBuffer");
            material_buffer.offset = 0;
            material_buffer.size = buffer_desc.size;
            should_reupload_data = true;
        }

        if (scene->updated_materials.size() > total_materials / 2 || frame_id == 0) {
            uint32_t material_instance_size = cast_u32(sizeof(Material3D::Properties));
            uint32_t total_materials_size_bytes = cast_u32(total_materials * material_instance_size);

            BufferView material_staging_buffer = per_frame_allocator[frame_flight_index].allocate(total_materials_size_bytes);
            Material3D::Properties *material_array = reinterpret_cast<Material3D::Properties *>(material_staging_buffer.ptr);

            jobsystem::Dispatch(total_materials, 64, [&](jobsystem::JobDispatchArg arg) {
                auto &material = scene->materials[arg.job_index];
                material_array[arg.job_index] = material->properties;
            });

            jobsystem::Wait();

            BufferCopyRegion copy_region = {
                .src_offset = material_staging_buffer.offset,
                .dst_offset = 0,
                .size = total_materials_size_bytes,
            };
            command_buffer->copy_buffer(material_buffer.buffer, material_staging_buffer.buffer, &copy_region, 1);

        } else {
            uint32_t total_updated_materials = cast_u32(scene->updated_materials.size());

            uint32_t element_size = cast_u32(sizeof(Material3D::Properties));
            ASSERT(element_size % 4 == 0);

            uint32_t allocation_size = total_updated_materials * element_size;
            BufferView src_buffer = per_frame_allocator[frame_flight_index].allocate(allocation_size);
            Material3D::Properties *material_array = reinterpret_cast<Material3D::Properties *>(src_buffer.ptr);

            uint32_t patch_buffer_allocation_size = cast_u32(sizeof(BufferPatch) * total_updated_materials);
            BufferView patch_buffer = per_frame_allocator[frame_flight_index].allocate(patch_buffer_allocation_size);
            BufferPatch *patch_array = reinterpret_cast<BufferPatch *>(patch_buffer.ptr);

            element_size /= 4;

            for (uint32_t i = 0; i < total_updated_materials; ++i) {
                uint32_t material_index = scene->updated_materials[i];
                material_array[i] = scene->materials[material_index]->properties;

                uint32_t offset = material_index * element_size;
                patch_array[i] = BufferPatch{offset, element_size};
            }

            dispatch_patch_copy(command_buffer, patch_buffer, src_buffer, material_buffer, total_updated_materials);
        }

        scene->updated_materials.clear();
    }

    void Renderer::prepare_buffer_for_shader_read(CommandBuffer *command_buffer) {
        std::vector<BufferBarrierInfo> barrier_infos(3);
        barrier_infos[0] = {transform_buffer.buffer, 0, UINT64_MAX, PIPELINE_STAGE_VERTEX_SHADER_BIT | PIPELINE_STAGE_COMPUTE_SHADER_BIT, ACCESS_FLAG_SHADER_READ};
        barrier_infos[1] = {material_buffer.buffer, 0, UINT64_MAX, PIPELINE_STAGE_FRAGMENT_SHADER_BIT, ACCESS_FLAG_SHADER_READ};
        barrier_infos[2] = {light_buffer.buffer, 0, UINT64_MAX, PIPELINE_STAGE_FRAGMENT_SHADER_BIT | PIPELINE_STAGE_COMPUTE_SHADER_BIT, ACCESS_FLAG_SHADER_READ};
        command_buffer->prepare_buffer(barrier_infos.data(), cast_u32(barrier_infos.size()));

        DescriptorInfo descriptor_infos[] = {
            {DescriptorType::StorageBuffer, transform_buffer.buffer, {0, UINT64_MAX}},
            {DescriptorType::StorageBuffer, material_buffer.buffer, {0, UINT64_MAX}},
            {DescriptorType::StorageBuffer, light_buffer.buffer, {0, UINT64_MAX}},
        };
        transform_descriptor = resource_heap.push_descriptors_per_frame(device.get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
        material_descriptor = transform_descriptor + 1;
        light_descriptor = transform_descriptor + 2;
    }

    void Renderer::upload_lights(CommandBuffer *command_buffer) {
        if (scene->updated_lights.size() == 0 && frame_id > 0)
            return;

        auto get_gpu_light = [](LightComponent *light, TransformComponent *transform) {
            uint32_t flag = light->light_type | (uint32_t(light->cast_shadow) << 3);
            if (light->light_type == LIGHT_TYPE_DIRECTIONAL) {
                return GPULightData{
                    .flag = flag,
                    .direction = quat_to_direction(transform->rotation),
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                };
            } else if (light->light_type == LIGHT_TYPE_POINT) {
                return GPULightData{
                    .position = transform->position,
                    .flag = flag,
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                    .radius_or_height = light->radius,
                };
            } else if (light->light_type == LIGHT_TYPE_SPOT) {
                glm::vec3 direction = quat_to_direction(transform->rotation);
                float radius = light->height * tan(light->outer_cone_angle);
                return GPULightData{
                    .position = transform->position,
                    .flag = flag,
                    .direction = direction,
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                    .radius_or_height = light->height,
                    .inner_angle = light->inner_cone_angle,
                    .outer_angle = light->outer_cone_angle,
                };
            } else {
                ASSERT_MSG(0, "Unknown light type");
                return GPULightData{};
            }
        };

        auto &component_manager = scene->ecs->component_manager;
        const auto &light_comp_array = component_manager->get_component_array<LightComponent>();

        total_lights = cast_u32(light_comp_array->components.size());
        // ASSERT(total_lights <= AppSettings::K_MAX_LIGHTS);
        if (scene->updated_lights.size() > total_lights / 2 || frame_id == 0) {
            uint64_t total_light_size_bytes = total_lights * sizeof(GPULightData);
            BufferView light_staging_buffer = per_frame_allocator[frame_flight_index].allocate(total_light_size_bytes);
            GPULightData *light_array = reinterpret_cast<GPULightData *>(light_staging_buffer.ptr);

            jobsystem::Dispatch(total_lights, 64, [&](jobsystem::JobDispatchArg arg) {
                Entity entity = light_comp_array->entities[arg.job_index];
                LightComponent *light = light_comp_array->get_component(entity);
                TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
                light_array[arg.job_index] = get_gpu_light(light, transform);
            });

            jobsystem::Wait();
            BufferCopyRegion copy_region = {
                .src_offset = light_staging_buffer.offset,
                .dst_offset = 0,
                .size = total_light_size_bytes,
            };
            command_buffer->copy_buffer(light_buffer.buffer, light_staging_buffer.buffer, &copy_region, 1);

        } else {
            ASSERT(0);
        }

        scene->updated_lights.clear();
    }

    void Renderer::initialize_scene_default_meshes(CommandBuffer *command_buffer) {
        // Create Plane Mesh
        auto create_mesh_allocation = [&](const MeshData &mesh_data, AABB aabb) {
            uint32_t vertex_data_size = cast_u32(mesh_data.vertices.size() * sizeof(VertexData));
            uint32_t index_data_size = cast_u32(mesh_data.indices.size() * sizeof(uint32_t));
            uint32_t total_data_size = vertex_data_size + index_data_size;
            BufferView geometry_buffer = geometry_buffer_allocator->allocate(total_data_size);

            uint32_t mesh_index = cast_u32(scene->mesh_allocations.size());
            scene->mesh_allocations.emplace_back(MeshAllocation{
                .buffer = geometry_buffer.buffer,
                .vertex_offset_bytes = geometry_buffer.offset,
                .vertex_stride = K_VERTEX_DATA_SIZE,
                .vertex_count = cast_u32(mesh_data.vertices.size()),
                .index_offset_bytes = geometry_buffer.offset + vertex_data_size,
                .index_count = cast_u32(mesh_data.indices.size()),
                .ouput_vertex_offset_bytes = 0,
                .local_aabb = aabb,
                .blas = {.as = AccelerationStructureID{K_INVALID_ID}},
            });

            BufferView staging_buffer = per_frame_allocator[frame_flight_index].allocate(total_data_size);
            std::memcpy(staging_buffer.ptr, mesh_data.vertices.data(), vertex_data_size);
            std::memcpy(staging_buffer.ptr + vertex_data_size, mesh_data.indices.data(), index_data_size);

            BufferCopyRegion copy_region = {
                .src_offset = staging_buffer.offset,
                .dst_offset = geometry_buffer.offset,
                .size = total_data_size,
            };
            command_buffer->copy_buffer(geometry_buffer.buffer, staging_buffer.buffer, &copy_region, 1);
            return mesh_index;
        };

        MeshData mesh_data;
        {
            generate_plane_mesh(&mesh_data);
            scene->plane_mesh_index = create_mesh_allocation(mesh_data, AABB{glm::vec3(-0.5f, -0.01f, -0.5f), glm::vec3(0.5f, 0.01f, 0.5f)});
        }
        {
            generate_cube_mesh(&mesh_data);
            scene->cube_mesh_index = create_mesh_allocation(mesh_data, AABB{glm::vec3(-0.5f), glm::vec3(0.5f)});
        }
        {
            generate_sphere_mesh(&mesh_data);
            scene->sphere_mesh_index = create_mesh_allocation(mesh_data, AABB{glm::vec3(-0.5f), glm::vec3(0.5f)});
        }
    }

    void Renderer::dispatch_patch_copy(CommandBuffer *command_buffer, const BufferView &patch_data_buffer, const BufferView &src_buffer, const BufferView &dst_buffer, uint32_t total_patches) {
        BufferBarrierInfo barriers[] = {
            /*{patch_data_buffer.buffer, patch_data_buffer.offset, patch_data_buffer.size, PIPELINE_STAGE_COMPUTE_SHADER_BIT, ACCESS_FLAG_SHADER_READ},
            {src_buffer.buffer, src_buffer.offset, src_buffer.size, PIPELINE_STAGE_COMPUTE_SHADER_BIT, ACCESS_FLAG_SHADER_READ},*/
            {dst_buffer.buffer, dst_buffer.offset, dst_buffer.size, PIPELINE_STAGE_COMPUTE_SHADER_BIT, ACCESS_FLAG_SHADER_WRITE},
        };

        command_buffer->prepare_buffer(barriers, cast_u32(std::size(barriers)));

        DescriptorInfo descriptor_infos[] = {
            {DescriptorType::StorageBuffer, src_buffer.buffer, {src_buffer.offset, src_buffer.size}},
            {DescriptorType::StorageBuffer, patch_data_buffer.buffer, {patch_data_buffer.offset, patch_data_buffer.size}},
            {DescriptorType::StorageBuffer, dst_buffer.buffer, {dst_buffer.offset, dst_buffer.size}},
        };
        DescriptorOffset descriptor = resource_heap.push_descriptors_per_frame(device.get(), descriptor_infos, 3);

        DescriptorOffset descriptors[] = {
            descriptor,
            descriptor + 1,
            descriptor + 2,
        };

        uint32_t push_data[] = {total_patches, 0, 0, 0};
        uint32_t push_data_size = cast_u32(sizeof(push_data));

        patch_buffer_shader->bind(command_buffer);
        command_buffer->set_push_data(0, &push_data, push_data_size);
        command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

        uint32_t work_group_size = rendering_utils::get_workgroup_size(total_patches, 32);
        command_buffer->dispatch(work_group_size, 1, 1);
    } // namespace mirai

    void Renderer::create_batches() {
        ScopedCpuProfiling("Create Batch");
        main_render_batches.clear();

        Camera *camera = scene->get_camera();
        const FrustumPlanes &frustum_planes = freeze_frustum ? freezed_frustum_planes : camera->get_frustum_planes();
        DrawBatchGenerator::BuildBatches(scene.get(), BatchBuildParams{
                                                          .frustum = &frustum_planes,
                                                          .camera_position = &camera->position,
                                                      },
                                         main_render_batches);

        if (!freeze_frustum) {
            freezed_frustum_planes = frustum_planes;
            freezed_inv_VP = camera->get_inv_view_projection_transform();
        }
    }

    void Renderer::create_blas() {
        if (!device->supports_raytracing()) {
            tlas.as = AccelerationStructureID{K_INVALID_ID};
            tlas_buffer = BufferID{K_INVALID_ID};
            return;
        }

        auto &component_manager = scene->ecs->component_manager;
        auto mesh_component_ptr = component_manager->get_component_array<MeshComponent>();

        uint32_t total_mesh_static = 0;
        uint32_t total_mesh_skinned = 0;
        // We only want to create BLAS for static object in this stage, dynamic object
        // are handled every frame
        std::vector<MeshAllocation> &mesh_allocations = scene->mesh_allocations;
        uint32_t total_meshes = cast_u32(mesh_allocations.size());

        for (const auto &allocation : mesh_allocations) {
            if (allocation.vertex_stride == K_VERTEX_DATA_SIZE_SKINNED)
                total_mesh_skinned += 1;
            else
                total_mesh_static += 1;
        }

        std::vector<AccelerationStructure *> out_blas_static(total_mesh_static);
        std::vector<BLASDescription> blas_descriptions_static(total_mesh_static);

        std::vector<AccelerationStructure *> out_blas_dynamic(total_mesh_skinned);
        std::vector<BLASDescription> blas_descriptions_dynamic(total_mesh_skinned);
        total_mesh_static = 0;
        total_mesh_skinned = 0;

        for (uint32_t i = 0; i < total_meshes; ++i) {
            MeshAllocation &allocation = mesh_allocations[i];

            AccelerationStructure **blas = nullptr;
            BLASDescription *blas_desc = nullptr;
            if (allocation.vertex_stride == K_VERTEX_DATA_SIZE_SKINNED) {
                blas = &out_blas_dynamic[total_mesh_skinned];
                blas_desc = &blas_descriptions_dynamic[total_mesh_skinned++];
            } else {
                blas = &out_blas_static[total_mesh_static];
                blas_desc = &blas_descriptions_static[total_mesh_static++];
            }
            *blas = &allocation.blas;
            blas_desc->vertex_buffer = allocation.buffer;
            blas_desc->index_buffer = allocation.buffer;
            blas_desc->vertex_offset = allocation.vertex_offset_bytes;
            blas_desc->index_offset = allocation.index_offset_bytes;
            blas_desc->vertex_stride = allocation.vertex_stride;
            blas_desc->vertex_count = allocation.vertex_count;
            blas_desc->index_count = allocation.index_count;
        }

        if (total_mesh_static > 0)
            device->create_blas(blas_descriptions_static, out_blas_static, blas_buffer_static, ASC_ALLOW_COMPACTION_BIT_KHR | ASC_PREFER_FAST_TRACE_BIT_KHR);
        if (total_mesh_skinned > 0)
            device->create_blas(blas_descriptions_dynamic, out_blas_dynamic, blas_buffer_dynamic, ASC_ALLOW_UPDATE_BIT_KHR | ASC_PREFER_FAST_BUILD_BIT_KHR);
    }

    void Renderer::create_tlas(CommandBuffer *command_buffer) {
        if (device->supports_raytracing()) {
            ScopedCpuProfiling("TLAS Build CPU");
            ScopedGpuProfiling(command_buffer, "TLAS Build");

            uint32_t instance_count = cast_u32(scene->render_object_list.size());
            uint32_t instance_data_size = cast_u32(sizeof(AccelerationStructureInstanceData));
            BufferView instance_buffer = per_frame_allocator[frame_flight_index].allocate(instance_count * instance_data_size);
            uint64_t blas_device_address = device->get_buffer_device_address(blas_buffer_static);

            // Copy TLAS instance data
            auto &component_manager = scene->ecs->component_manager;
            jobsystem::Dispatch(instance_count, 64, [&](jobsystem::JobDispatchArg arg) {
                AccelerationStructureInstanceData *instance = reinterpret_cast<AccelerationStructureInstanceData *>(instance_buffer.ptr + arg.job_index * instance_data_size);
                TransformComponent *transform_component = component_manager->get_component<TransformComponent>(scene->render_object_list[arg.job_index].entity);
                // The default representation of glm is column major while the VkTransformKHR uses row major
                // glm::mat4 transform = glm::transpose(transform_component.world_transform);
                glm::mat4 transform = transform_component->world_transform;
                for (int y = 0; y < 3; ++y) {
                    for (int x = 0; x < 4; ++x) {
                        instance->matrix[y][x] = transform[x][y];
                    }
                }
                instance->instanceCustomIndex = 0;
                instance->mask = 0xFF;
                instance->instanceShaderBindingTableRecordOffset = 0;
                instance->flags = GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT;
                instance->accelerationStructureReference = scene->render_object_list[arg.job_index].blas_buffer_device_address;
            });
            jobsystem::Wait();

            device->create_tlas(command_buffer, instance_count, instance_buffer, tlas_buffer, &tlas);
        }
    }

    void Renderer::update_skinned_mesh(CommandBuffer *command_buffer) {
        /**
         * This function is responsible for to
         * 1. Updating each skinned mesh in compute shader and generate final animated vertices
         * 2. Refit BLAS for all the skinned mesh
         */

        if (scene->animation_players.size() == 0)
            return;

        auto &component_manager = scene->ecs->component_manager;
        auto animator_component_ptr = component_manager->get_component_array<AnimatorComponent>();
        if (animator_component_ptr->components.size() == 0)
            return;

        ScopedCpuProfiling("ComputeSkinningSetup");

        // Upload skinning matrix
        std::vector<uint32_t> skinned_matrix_prefix_sum(scene->animation_players.size());
        uint32_t skinned_matrix_size = 0;
        for (uint32_t i = 0; i < scene->animation_players.size(); ++i) {
            std::unique_ptr<AnimationPlayer> &animation_player = scene->animation_players[i];

            skinned_matrix_size += cast_u32(animation_player->matrix_palletes.size());
            skinned_matrix_prefix_sum[i] = skinned_matrix_size;
        }
        ASSERT(skinned_matrix_size > 0);

        BufferView matrix_pallete_buffer = per_frame_allocator[frame_flight_index].allocate(cast_u32(skinned_matrix_size * sizeof(glm::mat4)));
        uint8_t *ptr = matrix_pallete_buffer.ptr;
        for (const auto &animation_player : scene->animation_players) {
            const std::vector<glm::mat4> &matrix_pallete = animation_player->matrix_palletes;
            uint32_t matrix_pallete_size = cast_u32(sizeof(glm::mat4) * matrix_pallete.size());
            std::memcpy(ptr, matrix_pallete.data(), matrix_pallete_size);
            ptr += matrix_pallete_size;
        }

        // List all the animated mesh
        struct SkinnedMeshPushData {
            uint32_t vertex_address;
            uint32_t vertex_stride;
            uint32_t vertex_count;
            uint32_t output_offset;

            uint32_t matrix_palletes_offset;
            uint32_t matrix_pallete_count;
            uint32_t _padding[2];
        };

        HashMap<uint32_t, std::vector<SkinnedMeshPushData>> skinned_mesh_data;
        std::vector<BLASDescription> blas_descriptions;
        std::vector<AccelerationStructureID> blases;

        for (auto entity : animator_component_ptr->entities) {
            MeshComponent *mesh_component = component_manager->get_component<MeshComponent>(entity);
            ASSERT(mesh_component != nullptr);

            AnimatorComponent *animator_component = component_manager->get_component<AnimatorComponent>(entity);
            for (uint32_t p = 0; p < mesh_component->primitives.size(); ++p) {
                const Primitive &primitive = mesh_component->primitives[p];
                const MeshAllocation &allocation = scene->mesh_allocations[primitive.mesh];

                // Vertices is access as uint in the shader, so the offset/stride should be
                // in the sizeof uint instead of bytes
                uint32_t animation_player_index = animator_component->animation_player_index;
                uint32_t pallete_offset = animation_player_index == 0 ? 0 : skinned_matrix_prefix_sum[animation_player_index - 1];
                uint32_t pallete_count = skinned_matrix_prefix_sum[animation_player_index] - pallete_offset;

                skinned_mesh_data[allocation.buffer.id].emplace_back(SkinnedMeshPushData{
                    .vertex_address = cast_u32(allocation.vertex_offset_bytes / 4),
                    .vertex_stride = allocation.vertex_stride / 4,
                    .vertex_count = allocation.vertex_count,
                    .output_offset = cast_u32(allocation.ouput_vertex_offset_bytes / 4),
                    // Access as mat4 in shader, so we don't convert it to bytes
                    .matrix_palletes_offset = pallete_offset,
                    .matrix_pallete_count = pallete_count,
                });
                blas_descriptions.emplace_back(BLASDescription{
                    .vertex_buffer = allocation.buffer,
                    .index_buffer = allocation.buffer,
                    .vertex_offset = allocation.ouput_vertex_offset_bytes,
                    .index_offset = allocation.index_offset_bytes,
                    .vertex_stride = K_VERTEX_DATA_SIZE,
                    .vertex_count = allocation.vertex_count,
                    .index_count = allocation.index_count,
                });
                blases.push_back(allocation.blas.as);
            }
        }

        {
            ScopedGpuProfiling(command_buffer, "SkinningCS");

            command_buffer->begin_gpu_debug_label("SkinningCS");

            DescriptorInfo matrix_pallete_descriptor_info = {
                .type = DescriptorType::StorageBuffer,
                .resource = matrix_pallete_buffer.buffer,
                .buffer_info = {
                    .offset = matrix_pallete_buffer.offset,
                    .size = matrix_pallete_buffer.size,
                },
            };

            std::vector<BufferBarrierInfo> barriers;

            DescriptorOffset matrix_pallete_descriptor = resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &matrix_pallete_descriptor_info, 1);
            skinning_shader->bind(command_buffer);

            for (auto &[key, value] : skinned_mesh_data) {
                BufferID buffer_id = BufferID{key};
                BufferBarrierInfo barrier_info = {
                    .buffer_id = buffer_id,
                    .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
                };
                command_buffer->prepare_buffer(&barrier_info, 1);

                DescriptorOffset descriptors[] = {
                    get_or_create_descriptor(buffer_id, DescriptorType::StorageBuffer),
                    matrix_pallete_descriptor,
                };
                command_buffer->set_push_data(cast_u32(sizeof(uint32_t) * 8), descriptors, cast_u32(sizeof(descriptors)));

                for (const auto &push_data : value) {
                    command_buffer->set_push_data(0, &push_data, cast_u32(sizeof(push_data)));
                    command_buffer->dispatch(push_data.vertex_count, 1, 1);
                }

                barriers.push_back({
                    .buffer_id = buffer_id,
                    .dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT,
                    .dst_access_mask = ACCESS_FLAG_SHADER_READ,
                });
            }

            command_buffer->prepare_buffer(barriers.data(), cast_u32(barriers.size()));
            command_buffer->end_gpu_debug_label();
        }

        // Update BLAS For skinning mesh
        // We don't have any barrier related to this here, we put an aggregate memory barrier barrier after
        // TLAS creation
        ScopedGpuProfiling(command_buffer, "BLAS Build");
        device->refit_blas(command_buffer, blas_descriptions, blases, blas_buffer_dynamic, ASC_ALLOW_UPDATE_BIT_KHR | ASC_PREFER_FAST_BUILD_BIT_KHR);
    }

    void Renderer::upload_batch_data(std::vector<RenderBatch> &batches, uint32_t current_frame) {
        if (batches.size() == 0)
            return;
        // Calculate total memory required in staging buffer
        uint32_t total_entities = 0;
        uint32_t draw_indirect_size_bytes = 0;

        for (auto &batch : batches) {
            uint32_t num_entity = cast_u32(batch.draw_infos.size());
            total_entities += num_entity;
            draw_indirect_size_bytes += sizeof(DrawIndexedIndirectCommand) * num_entity;
        }

        if (total_entities == 0)
            return;

        uint32_t draw_data_instance_size = sizeof(uint32_t) * 4;
        uint32_t draw_data_size_bytes = total_entities * draw_data_instance_size;

        BufferView draw_data_buffer = per_frame_allocator[current_frame].allocate(draw_data_size_bytes);
        uint8_t *draw_data_array = draw_data_buffer.ptr;

        BufferView draw_indirect_buffer = per_frame_allocator[current_frame].allocate(draw_indirect_size_bytes);
        uint8_t *draw_indirect_array = draw_indirect_buffer.ptr;

        uint64_t batch_draw_data_offset = draw_data_buffer.offset;
        uint64_t batch_draw_indirect_data_offset = draw_indirect_buffer.offset;

        DescriptorInfo draw_data_descriptor_info = {
            .type = DescriptorType::StorageBuffer,
            .resource = draw_data_buffer.buffer,
        };

        for (auto &batch : batches) {
            uint32_t num_entity = cast_u32(batch.draw_infos.size());

            // Update descriptor info
            draw_data_descriptor_info.buffer_info = {.offset = batch_draw_data_offset, .size = num_entity * draw_data_instance_size};

            batch.draw_indirect_buffer_view.buffer = draw_indirect_buffer.buffer;
            batch.draw_indirect_buffer_view.offset = batch_draw_indirect_data_offset;
            batch.draw_indirect_buffer_view.size = sizeof(DrawIndexedIndirectCommand) * num_entity;

            for (uint32_t e = 0; e < num_entity; ++e) {
                MeshDrawInfo &draw_info = batch.draw_infos[e];
                uint32_t draw_data[] = {
                    draw_info.transform_index,
                    draw_info.material_index,
                    cast_u32(draw_info.draw_info.vertex_offset_bytes / 4),
                    draw_info.vertex_stride / 4, // Convert stride to uint32 offset
                };
                std::memcpy(draw_data_array, &draw_data, draw_data_instance_size);
                draw_data_array += draw_data_instance_size;
                draw_info.draw_info.vertex_offset_bytes = 0;

                std::memcpy(draw_indirect_array, &draw_info.draw_info, sizeof(DrawIndexedIndirectCommand));
                draw_indirect_array += sizeof(DrawIndexedIndirectCommand);
            }

            // Create per frame batch draw data descriptor
            batch.draw_data_descriptor = resource_heap.push_descriptors_per_frame(device.get(), &draw_data_descriptor_info, 1);

            batch_draw_indirect_data_offset += sizeof(DrawIndexedIndirectCommand) * num_entity;
            batch_draw_data_offset += draw_data_instance_size * num_entity;
        }

        total_visible_entities = total_entities;
    }

    DescriptorOffset Renderer::get_or_create_descriptor(ID resource_id, DescriptorType descriptor_type) {
        uint64_t key = resource_id.id;
        key = key << 32 | uint32_t(descriptor_type);
        auto found = descriptor_map.find(key);
        if (found != descriptor_map.end()) {
            return found->second;
        } else {
            DescriptorInfo descriptor_info = {
                .type = descriptor_type,
                .resource = resource_id,
            };

            if (descriptor_type == DescriptorType::UniformBuffer || descriptor_type == DescriptorType::StorageBuffer) {
                descriptor_info.buffer_info = {0, ~0u};
            } else if (descriptor_type == DescriptorType::SampledImage || descriptor_type == DescriptorType::StorageImage) {
                descriptor_info.image_info = {0, ~0u, 0, ~0u};
            }

            DescriptorOffset offset = resource_heap.push_descriptors(device.get(), &descriptor_info, 1);
            descriptor_map.insert(std::make_pair(key, offset));
            return offset;
        }
    }

    DescriptorOffset Renderer::get_or_create_descriptor(const std::string &name, const DescriptorInfo &descriptor_info) {
        uint64_t key = utils::djb2_hash_string(name);
        // Don't need to do this
        key = key << 32 | uint32_t(descriptor_info.type);
        auto found = descriptor_map.find(key);
        if (found != descriptor_map.end()) {
            return found->second;
        } else {
            DescriptorOffset offset = resource_heap.push_descriptors(device.get(), &descriptor_info, 1);
            descriptor_map.insert(std::make_pair(key, offset));
            return offset;
        }
    }

    void Renderer::upload_per_frame_data() {
        GPULinearAllocator *frame_allocator = &per_frame_allocator[frame_flight_index];

        // Copy per frame uniform data
        uint32_t per_frame_data_size = align_memory(cast_u32(sizeof(Scene::FrameData)), 64);
        BufferView per_frame_data_buffer = frame_allocator->allocate(per_frame_data_size);
        std::memcpy(per_frame_data_buffer.ptr, &scene->per_frame_data, per_frame_data_size);

        // Update per frame data descriptor
        DescriptorInfo descriptor_info = {
            .type = DescriptorType::UniformBuffer,
            .resource = per_frame_data_buffer.buffer,
            .buffer_info = {per_frame_data_buffer.offset, per_frame_data_buffer.size},
        };
        per_frame_data_descriptor = resource_heap.push_descriptors_per_frame(device.get(), &descriptor_info, 1);

        // Update cascade data
        uint32_t cascade_data_size = align_memory(sizeof(shadow_system->cascade_info), 64);
        BufferView cascade_data_buffer = frame_allocator->allocate(cascade_data_size);
        std::memcpy(cascade_data_buffer.ptr, &shadow_system->cascade_info, cascade_data_size);
        descriptor_info.buffer_info.offset = cascade_data_buffer.offset;
        descriptor_info.buffer_info.size = cascade_data_size;
        cascade_data_descriptor = resource_heap.push_descriptors_per_frame(device.get(), &descriptor_info, 1);

        // Populate per-frame batch data
        upload_batch_data(main_render_batches, frame_flight_index);

        DescriptorInfo descriptor_infos[] = {
            {DescriptorType::UniformBuffer, per_frame_data_buffer.buffer, {per_frame_data_buffer.offset, per_frame_data_buffer.size}},
            {DescriptorType::UniformBuffer, cascade_data_buffer.buffer, {cascade_data_buffer.offset, cascade_data_buffer.size}},
        };
        per_frame_data_descriptor = resource_heap.push_descriptors_per_frame(device.get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
        cascade_data_descriptor = per_frame_data_descriptor + 1;
    }

    void Renderer::add_bindless_texture(TextureID texture) {
        ASSERT(texture.is_valid() && texture.id < AppSettings::K_MAX_BINDLESS_TEXTURE_COUNT);
        ASSERT(bindless_texture_count < AppSettings::K_MAX_BINDLESS_TEXTURE_COUNT);

        DescriptorInfo descriptor_info = {
            .type = DescriptorType::SampledImage,
            .resource = texture,
            .image_info = {0, UINT32_MAX, 0, UINT32_MAX},
        };

        resource_heap.push_descriptor_at_index(device.get(), &descriptor_info, 1, texture.id);
        bindless_texture_count++;
    }

    void Renderer::update() {
        ScopedCpuProfiling("Update renderer");
        frame_flight_index = device->get_current_frame_in_flight_index();

        resource_heap.new_frame(frame_flight_index);
        line_renderer->new_frame(frame_flight_index);

        scene->update();

        // Generate and Upload visible lights
        create_batches();

        shadow_system->update(scene.get());

        // Debug Data
        if (freeze_frustum) {
            FrustumPoints frustum_points;
            frustum_points.create_from_matrix(freezed_inv_VP);
            line_renderer->add_frustum(frustum_points.points, 0x00ff00ff);
        }

        if (show_aabbs) {
            Camera *camera = scene->get_camera();
            const FrustumPlanes &frustum = freeze_frustum ? freezed_frustum_planes : camera->get_frustum_planes();
            uint32_t render_object_count = cast_u32(scene->render_object_list.size());
            for (uint32_t i = 0; i < render_object_count; ++i) {
                const RenderableObjectData &renderable = scene->render_object_list[i];
                if (!frustum.intersect_aabb(renderable.transformed_aabb))
                    continue;
                line_renderer->add_aabb(renderable.transformed_aabb, 0xf07314ff);
            }
        }
    }

    void copy_continuous_region(CommandBuffer *cb, const std::vector<uint32_t> &indices, BufferView src, BufferView dst, uint32_t data_element_size) {
        BufferBarrierInfo barrier_infos[] = {
            {
                .buffer_id = dst.buffer,
                .offset = dst.offset,
                .size = dst.size,
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
                .src_offset = src.offset + src_start_index * data_element_size,
                .dst_offset = dst.offset + dst_start_index * data_element_size,
                .size = range * data_element_size,
            });
            src_start_index = i;
            dst_start_index = indices[i];
        }

        uint32_t src_end_index = cast_u32(indices.size());
        uint32_t range = src_end_index - src_start_index;
        copy_regions.push_back({
            .src_offset = src.offset + src_start_index * data_element_size,
            .dst_offset = dst.offset + dst_start_index * data_element_size,
            .size = range * data_element_size,
        });
        cb->copy_buffer(dst.buffer, src.buffer, copy_regions.data(), cast_u32(copy_regions.size()));

        barrier_infos[0].dst_access_mask = ACCESS_FLAG_SHADER_READ;
        barrier_infos[0].dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT;
        cb->prepare_buffer(barrier_infos, 1);
    }

    void Renderer::render() {

        device->new_frame();

        ASSERT(frame_flight_index < AppSettings::K_MAX_FRAME_IN_FLIGHTS);
        per_frame_allocator[frame_flight_index].reset();

        CommandBuffer *cb = device->get_command_buffer();

        cb->begin();

        cb->bind_resource_heap(resource_heap.buffer);
        cb->bind_sampler_heap(sampler_heap.buffer);

        miProfiler::BeginFrame(cb);
        {
            ScopedCpuProfiling("CPU Render Time");
            ScopedGpuProfiling(cb, "GPU Time");

            // Copy per frame data from staging buffer to gpu uniform buffer
            // Transform/Material/Light descriptor are also updated here
            upload_per_frame_data();

            upload_transforms(cb);
            upload_materials(cb);
            upload_lights(cb);

            // Update skinned mesh
            update_skinned_mesh(cb);

            // create TLAS
            create_tlas(cb);

            RenderContext context{this, cb};

            prepare_buffer_for_shader_read(cb);

            frame_graph->execute(&context);

            if (frame_graph->get_present_texture() != K_SWAPCHAIN_TEXTURE_HANDLE)
                cb->copy_to_swapchain(frame_graph->get_present_texture());

            device->queue_command_buffer(cb);
        }
        frame_id++;
        device->present();
    }

    Renderer::~Renderer() {
        geometry_buffer_allocator->shutdown();

        for (uint32_t i = 0; i < AppSettings::K_MAX_FRAME_IN_FLIGHTS; ++i)
            per_frame_allocator[i].shutdown();

        BufferID buffers[] = {resource_heap.buffer, sampler_heap.buffer, light_buffer.buffer, transform_buffer.buffer, material_buffer.buffer};
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));

        if (device->supports_raytracing()) {
            for (auto &allocation : scene->mesh_allocations) {
                device->destroy_acceleration_structures(&allocation.blas.as, 1);
            }

            device->destroy_acceleration_structures(&tlas.as, 1);
            if (blas_buffer_dynamic.is_valid())
                device->destroy_buffers(&blas_buffer_dynamic, 1);
            if (blas_buffer_static.is_valid())
                device->destroy_buffers(&blas_buffer_static, 1);
            if (tlas_buffer.is_valid())
                device->destroy_buffers(&tlas_buffer, 1);
        }

        frame_graph.reset();
        frame_graph_blackboard.reset();
        line_renderer.reset();
        shader_registry->destroy();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai