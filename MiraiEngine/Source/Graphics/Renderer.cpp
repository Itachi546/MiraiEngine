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

    Renderer::Renderer() {
        ASSERT(Instance == nullptr);
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        texture_cache = std::make_unique<TextureCache>();
        line_renderer = std::make_unique<LineRenderer>();
        miProfiler::Initialize();

        // Preload shaders
        shader_registry_map = std::make_unique<ShaderRegistryMap>();
        preload_shaders();

        frame_graph = std::make_unique<FrameGraph>();
        frame_graph_blackboard = std::make_unique<FrameGraphBlackBoard>();
        // Add debug data to blackboard
        frame_graph_blackboard->add<RenderDebugData>(RenderDebugData{
            .split_percentage = 0.0f,
            .debug_param_index = 0,
            .show_debug_cascade_color = false,
            .enable_gamma_correction = true,
            .light_culling = true,
            .exposure = 1.0f,
            .mip_lod_bias = -0.5f,
        });

        shadow_system = std::make_unique<ShadowSystem>();
        current_frame_index = device->get_current_frame();
    }

    void Renderer::initialize() {
        // Allocate Geometry buffer
        vertex_buffer_allocator.init({
                                         .size = DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE,
                                         .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                         .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                     },
                                     "GlobalVertexBuffer");
        index_buffer_allocator.init({
                                        .size = DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE,
                                        .usage_flags = BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT,
                                        .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                    },
                                    "GlobalIndexBuffer");

        // Allocate per-frame-staging buffer
        // Per frame staging buffer
        uint32_t total_frames = device->get_swapchain_image_count();
        BufferDescription buffer_desc = {
            .size = k_staging_buffer_size_per_frame,
            .usage_flags = BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_UNIFORM_BUFFER_BIT | BUFFER_USAGE_INDIRECT_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT,
            .allocation_type = MEMORY_ALLOCATION_TYPE_CPU,
        };
        Log::Info("Total Staging Buffer Memory: ", utils::bytes_to_mb(buffer_desc.size), " mb");
        Log::Info("Total Staging Buffer Memory/PerFrame: ", utils::bytes_to_mb(buffer_desc.size * total_frames), " mb");

        for (uint32_t i = 0; i < total_frames; ++i) {
            per_frame_allocator[i].init(buffer_desc, "PerFrameAllocator" + std::to_string(i));
        }

        // Allocate global transform/material buffer
        buffer_desc.size = K_MAX_ENTITIES * sizeof(glm::mat4);
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_GPU;
        buffer_desc.usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        global_transform_buffer = device->create_buffer(&buffer_desc, "global_transform_buffer");

        buffer_desc.size = K_MAX_ENTITIES * cast_u32(sizeof(Material3D::Properties));
        global_material_buffer = device->create_buffer(&buffer_desc, "global_material_buffer");

        // Allocate descriptor heap buffer
        buffer_desc.usage_flags = BUFFER_USAGE_DESCRIPTOR_HEAP_BIT_EXT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        buffer_desc.allocation_type = MEMORY_ALLOCATION_TYPE_CPU;

        // Allocate resource heap
        uint32_t resource_descriptor_count = AppSettings::K_RESOURCE_DESCRIPTOR_LIMIT + AppSettings::K_MAX_FRAME_IN_FLIGHTS * AppSettings::K_PER_FRAME_RESOURCE_DESCRIPTOR_LIMIT;
        buffer_desc.size = device->calculate_resource_descriptors_size(resource_descriptor_count);
        resource_heap.buffer = device->create_buffer(&buffer_desc, "global_resource_heap");
        resource_heap.ptr = device->map_buffer(resource_heap.buffer);
        resource_heap.descriptor_size = device->get_resource_descriptor_size();
        resource_heap.size = buffer_desc.size;
        resource_heap.new_frame(current_frame_index);
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
    }

    void Renderer::on_load_resources() {
        // Trigger scene update so that transforms are updated
        // @TODO Fix this
        // scene->update();
        Camera *camera = scene->get_camera();
        freezed_inv_VP = camera->get_inv_view_projection_transform();
        freezed_frustum_planes = camera->get_frustum_planes();

        create_blas();
        /*
 // Create acceleration structure for scene
 auto &render_list = scene->render_object_list;
 uint32_t render_object_count = scene->render_object_count.load(std::memory_order_relaxed);

 std::vector<AccelerationStructureMeshInfo> mesh_infos;
 mesh_infos.reserve(render_object_count);
 for (uint32_t i = 0; i < render_object_count; ++i) {
     RenderableObjectData &object = render_list[i];
     auto &material = scene->materials[object.material_index];
     if (material->is_transparent())
         continue;

     mesh_infos.push_back(AccelerationStructureMeshInfo{
         .vertex_buffer = {
             .buffer = object.vertex_buffer,
             .offset = object.vertex_offset_bytes,
             .count = object.index_count,
             .stride = object.vertex_stride,
         },
         .index_buffer = {
             .buffer = object.index_buffer,
             .offset = cast_u32(object.first_index * sizeof(uint32_t)),
             .count = object.index_count,
             .stride = sizeof(uint32_t),
         },
         .transform = {},
     });

     AccelerationStructureMeshInfo &mesh_info = mesh_infos.back();
     TransformComponent *transform_component = component_manager->get_component<TransformComponent>(render_list[i].entity);
     // The default representation of glm is column major while the VkTransformKHR uses row major
     // glm::mat4 transform = glm::transpose(transform_component.world_transform);
     glm::mat4 transform = transform_component->world_transform;
     for (int y = 0; y < 3; ++y) {
         for (int x = 0; x < 4; ++x) {
             mesh_info.transform[y][x] = transform[x][y];
         }
     }
 }
 if (mesh_infos.size() > 0)
     device->create_acceleration_structure(mesh_infos.data(), cast_u32(mesh_infos.size()));
 */

        CommandBuffer *command_buffer = device->get_command_buffer(0);
        command_buffer->begin();
        command_buffer->begin_gpu_debug_label("CopyGlobalBuffer", nullptr);

        auto &component_manager = scene->ecs->component_manager;
        // Update global transform buffer
        auto transform_components_ptr = component_manager->get_component_array<TransformComponent>();
        std::size_t transform_size_bytes = transform_components_ptr->components.size() * sizeof(glm::mat4);

        if (transform_size_bytes > 0) {
            BufferView temp_buffer = per_frame_allocator[1].allocate(cast_u32(transform_size_bytes));
            uint8_t *transform_array = temp_buffer.ptr;

            for (auto &component : transform_components_ptr->components) {
                std::memcpy(transform_array, &component.world_transform[0][0], sizeof(glm::mat4));
                transform_array += sizeof(glm::mat4);
            }

            BufferCopyRegion copy_region = {
                .src_offset = temp_buffer.offset,
                .dst_offset = 0,
                .size = temp_buffer.size,
            };
            command_buffer->copy_buffer(global_transform_buffer, temp_buffer.buffer, &copy_region, 1);
        }

        // Update material buffer
        uint32_t material_instance_size = cast_u32(sizeof(Material3D::Properties));
        std::size_t material_size_bytes = scene->materials.size() * material_instance_size;
        if (material_size_bytes > 0) {
            BufferView temp_buffer = per_frame_allocator[1].allocate(cast_u32(material_size_bytes));
            uint8_t *material_array = temp_buffer.ptr;

            for (auto &mat : scene->materials) {
                std::memcpy(material_array, &mat->properties, material_instance_size);
                material_array += material_instance_size;
            }

            BufferCopyRegion copy_region = {
                .src_offset = temp_buffer.offset,
                .dst_offset = 0,
                .size = temp_buffer.size,
            };
            command_buffer->copy_buffer(global_material_buffer, temp_buffer.buffer, &copy_region, 1);
        }

        command_buffer->end_gpu_debug_label();
        device->submit_command_buffer_immediate(command_buffer);
        command_buffer->wait();

        // Update transform descriptor
        DescriptorInfo descriptor_infos[] = {
            {
                .type = DescriptorType::StorageBuffer,
                .resource = global_transform_buffer,
                .buffer_info = {0, UINT64_MAX},
            },
            {
                .type = DescriptorType::StorageBuffer,
                .resource = global_material_buffer,
                .buffer_info = {0, UINT64_MAX},
            },
            {
                .type = DescriptorType::StorageBuffer,
                .resource = vertex_buffer_allocator.buffer,
                .buffer_info = {0, UINT64_MAX},
            },
        };
        transform_descriptor = resource_heap.push_descriptors(device.get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
        material_descriptor = transform_descriptor + 1;
        global_geometry_descriptor = transform_descriptor + 2;

        frame_graph->compile();
    }

    void Renderer::upload_visible_lights() {
        // @TODO optimize this structure later
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

        auto &component_manager = scene->ecs->component_manager;
        const auto &light_array = component_manager->get_component_array<LightComponent>();
        const FrustumPlanes &frustum = scene->get_camera()->get_frustum_planes();

        std::vector<GPULightData> visible_lights;
        visible_lights.reserve(1000);
        uint32_t total_lights = cast_u32(light_array->entities.size());

        for (auto entity : light_array->entities) {
            LightComponent *light = component_manager->get_component<LightComponent>(entity);
            if (disable_punctual_lights && light->light_type != LIGHT_TYPE_DIRECTIONAL)
                continue;

            TransformComponent *transform = component_manager->get_component<TransformComponent>(entity);
            uint32_t flag = light->light_type | (uint32_t(light->cast_shadow) << 3);
            if (light->light_type == LIGHT_TYPE_DIRECTIONAL) {
                visible_lights.push_back(GPULightData{
                    .flag = flag,
                    .direction = quat_to_direction(transform->rotation),
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                });
            } else if (light->light_type == LIGHT_TYPE_POINT) {
                visible_lights.push_back(GPULightData{
                    .position = transform->position,
                    .flag = flag,
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                    .radius_or_height = light->radius,
                });
            } else if (light->light_type == LIGHT_TYPE_SPOT) {
                glm::vec3 direction = quat_to_direction(transform->rotation);
                float radius = light->height * tan(light->outer_cone_angle);
                visible_lights.push_back(GPULightData{
                    .position = transform->position,
                    .flag = flag,
                    .direction = direction,
                    .intensity = light->intensity,
                    .color = rgb_to_u32(&light->color[0]),
                    .radius_or_height = light->height,
                    .inner_angle = light->inner_cone_angle,
                    .outer_angle = light->outer_cone_angle,
                });
            } else {
                ASSERT_MSG(0, "Unknown light type");
            }
        }
        total_visible_lights = cast_u32(visible_lights.size());

        uint32_t light_data_size = cast_u32(total_visible_lights * sizeof(GPULightData));

        BufferView light_buffer = per_frame_allocator[current_frame_index].allocate(light_data_size);
        std::memcpy(light_buffer.ptr, visible_lights.data(), light_data_size);

        DescriptorInfo descriptor = {
            .type = DescriptorType::StorageBuffer,
            .resource = light_buffer.buffer,
            .buffer_info = {
                .offset = light_buffer.offset,
                .size = light_data_size,
            },
        };

        per_frame_light_descriptor = resource_heap.push_descriptors_per_frame(device.get(), &descriptor, 1);
    }

    void Renderer::create_batches() {
        ScopedCpuProfiling("Create Batch");
        main_render_batches.clear();

        Camera *camera = scene->get_camera();
        const FrustumPlanes &frustum_planes = freeze_frustum ? freezed_frustum_planes : camera->get_frustum_planes();
        DrawBatchGenerator::BuildBatches(scene.get(), BatchBuildParams{
                                                          .filter_flags = BATCH_FILTER_FLAG_OPAQUE | BATCH_FILTER_FLAG_ALPHA_MASK | BATCH_FILTER_FLAG_TRANSPARENT | BATCH_FILTER_FLAG_SKINNED,
                                                          .frustum = &frustum_planes,
                                                          .camera_position = &camera->position,
                                                      },
                                         main_render_batches);

        for (auto &batch : main_render_batches) {
            batch.sort();
        }

        std::sort(main_render_batches.begin(), main_render_batches.end(), [](const RenderBatch &lhs, const RenderBatch &rhs) {
            return lhs.batch_type < rhs.batch_type;
        });

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
        for (auto &component : mesh_component_ptr->components) {
            if (component.mesh_subsets[0].vertex_stride == K_VERTEX_DATA_SIZE_SKINNED)
                total_mesh_skinned += cast_u32(component.mesh_subsets.size());
            else
                total_mesh_static += cast_u32(component.mesh_subsets.size());
        }

        std::vector<AccelerationStructure *> out_blas_static(total_mesh_static);
        std::vector<BLASDescription> blas_descriptions_static(total_mesh_static);

        std::vector<AccelerationStructure *> out_blas_dynamic(total_mesh_skinned);
        std::vector<BLASDescription> blas_descriptions_dynamic(total_mesh_skinned);
        total_mesh_static = 0;
        total_mesh_skinned = 0;

        for (auto &component : mesh_component_ptr->components) {
            // We only want to create BLAS for static object in this stage, dynamic object
            // are handled every frame
            component.blases.resize(component.mesh_subsets.size());

            for (uint32_t i = 0; i < component.mesh_subsets.size(); ++i) {
                MeshComponent::MeshSubset &subset = component.mesh_subsets[i];

                AccelerationStructure **blas = nullptr;
                BLASDescription *blas_desc = nullptr;
                if (component.mesh_subsets[0].vertex_stride == K_VERTEX_DATA_SIZE_SKINNED) {
                    blas = &out_blas_dynamic[total_mesh_skinned];
                    blas_desc = &blas_descriptions_dynamic[total_mesh_skinned++];
                } else {
                    blas = &out_blas_static[total_mesh_static];
                    blas_desc = &blas_descriptions_static[total_mesh_static++];
                }
                *blas = &component.blases[i];
                blas_desc->vertex_buffer = component.vertex_buffer.buffer;
                blas_desc->index_buffer = component.index_buffer.buffer;
                blas_desc->vertex_offset = subset.vertex_offset_bytes;
                blas_desc->index_offset = subset.index_offset_bytes;
                blas_desc->vertex_stride = subset.vertex_stride;
                blas_desc->vertex_count = subset.vertex_count;
                blas_desc->index_count = subset.index_count;
            }
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

            uint32_t instance_count = scene->render_object_count.load();
            uint32_t instance_data_size = cast_u32(sizeof(AccelerationStructureInstanceData));
            BufferView instance_buffer = per_frame_allocator[current_frame_index].allocate(instance_count * instance_data_size);
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

        BufferView matrix_pallete_buffer = per_frame_allocator[current_frame_index].allocate(cast_u32(skinned_matrix_size * sizeof(glm::mat4)));
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

        std::vector<SkinnedMeshPushData> skinned_mesh_data;
        std::vector<BLASDescription> blas_descriptions;
        std::vector<AccelerationStructureID> blases;

        for (auto entity : animator_component_ptr->entities) {
            MeshComponent *mesh_component = component_manager->get_component<MeshComponent>(entity);
            ASSERT(mesh_component != nullptr);

            AnimatorComponent *animator_component = component_manager->get_component<AnimatorComponent>(entity);
            for (uint32_t s = 0; s < mesh_component->mesh_subsets.size(); ++s) {
                const MeshComponent::MeshSubset &subset = mesh_component->mesh_subsets[s];

                // Vertices is access as uint in the shader, so the offset/stride should be
                // in the sizeof uint instead of bytes
                uint32_t animation_player_index = animator_component->animation_player_index;
                uint32_t pallete_offset = animation_player_index == 0 ? 0 : skinned_matrix_prefix_sum[animation_player_index - 1];
                uint32_t pallete_count = skinned_matrix_prefix_sum[animation_player_index] - pallete_offset;
                skinned_mesh_data.emplace_back(SkinnedMeshPushData{
                    .vertex_address = subset.vertex_offset_bytes / 4,
                    .vertex_stride = subset.vertex_stride / 4,
                    .vertex_count = subset.vertex_count,
                    .output_offset = subset.output_vertex_offset_bytes / 4,
                    // Access as mat4 in shader, so we don't convert it to bytes
                    .matrix_palletes_offset = pallete_offset,
                    .matrix_pallete_count = pallete_count,
                });

                blas_descriptions.emplace_back(BLASDescription{
                    .vertex_buffer = mesh_component->vertex_buffer.buffer,
                    .index_buffer = mesh_component->index_buffer.buffer,
                    .vertex_offset = subset.output_vertex_offset_bytes,
                    .index_offset = subset.index_offset_bytes,
                    .vertex_stride = K_VERTEX_DATA_SIZE,
                    .vertex_count = subset.vertex_count,
                    .index_count = subset.index_count,
                });
                blases.push_back(mesh_component->blases[s].as);
            }
        }
        {
            ScopedGpuProfiling(command_buffer, "SkinningCS");

            BufferBarrierInfo barrier_info = {
                .buffer_id = vertex_buffer_allocator.buffer,
                .dst_stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                .dst_access_mask = ACCESS_FLAG_SHADER_READ | ACCESS_FLAG_SHADER_WRITE,
            };
            command_buffer->prepare_buffer(&barrier_info, 1);

            command_buffer->begin_gpu_debug_label("SkinningCS");

            DescriptorInfo matrix_pallete_descriptor_info = {
                .type = DescriptorType::StorageBuffer,
                .resource = matrix_pallete_buffer.buffer,
                .buffer_info = {
                    .offset = matrix_pallete_buffer.offset,
                    .size = matrix_pallete_buffer.size,
                },
            };

            DescriptorOffset descriptors[] = {
                global_geometry_descriptor,
                resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &matrix_pallete_descriptor_info, 1),
            };

            skinning_shader->bind(command_buffer);
            command_buffer->set_push_data(cast_u32(sizeof(uint32_t) * 8), descriptors, cast_u32(sizeof(descriptors)));

            for (auto &data : skinned_mesh_data) {
                command_buffer->set_push_data(0, &data, cast_u32(sizeof(data)));
                command_buffer->dispatch(data.vertex_count, 1, 1);
            }

            barrier_info.dst_access_mask = ACCESS_FLAG_SHADER_READ;
            barrier_info.dst_stage_mask = PIPELINE_STAGE_VERTEX_SHADER_BIT | PIPELINE_STAGE_COMPUTE_SHADER_BIT;

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
            for (auto &mesh_batch : batch.meshes) {
                uint32_t num_entity = cast_u32(mesh_batch.mesh_draw_infos.size());
                total_entities += num_entity;
                draw_indirect_size_bytes += sizeof(DrawIndexedIndirectCommand) * num_entity;
            }
        }

        if (total_entities == 0)
            return;

        uint32_t draw_data_instance_size = sizeof(uint32_t) * 4;
        uint32_t draw_data_size_bytes = total_entities * draw_data_instance_size;

        BufferView draw_data_buffer = per_frame_allocator[current_frame].allocate(draw_data_size_bytes);
        uint8_t *draw_data_array = draw_data_buffer.ptr;

        BufferView draw_indirect_buffer = per_frame_allocator[current_frame].allocate(draw_indirect_size_bytes);
        uint8_t *draw_indirect_array = draw_indirect_buffer.ptr;

        uint32_t batch_draw_data_offset = draw_data_buffer.offset;
        uint32_t batch_draw_indirect_data_offset = draw_indirect_buffer.offset;
        for (auto &render_batch : batches) {
            for (auto &batch : render_batch.meshes) {
                uint32_t num_entity = cast_u32(batch.mesh_draw_infos.size());

                batch.draw_data_buffer_view.buffer = draw_data_buffer.buffer;
                batch.draw_data_buffer_view.offset = batch_draw_data_offset;
                batch.draw_data_buffer_view.size = num_entity * draw_data_instance_size;

                batch.draw_indirect_buffer_view.buffer = draw_indirect_buffer.buffer;
                batch.draw_indirect_buffer_view.offset = batch_draw_indirect_data_offset;
                batch.draw_indirect_buffer_view.size = sizeof(DrawIndexedIndirectCommand) * num_entity;

                for (uint32_t e = 0; e < num_entity; ++e) {
                    MeshDrawInfo &draw_info = batch.mesh_draw_infos[e];
                    uint32_t draw_data[] = {
                        draw_info.transform_index,
                        draw_info.material_index,
                        draw_info.draw_info.vertex_offset_bytes / 4,
                        draw_info.vertex_stride / 4, // Convert stride to uint32 offset
                    };
                    std::memcpy(draw_data_array, &draw_data, draw_data_instance_size);
                    draw_data_array += draw_data_instance_size;
                    // @TODO Fix this
                    draw_info.draw_info.vertex_offset_bytes = 0;

                    std::memcpy(draw_indirect_array, &draw_info.draw_info, sizeof(DrawIndexedIndirectCommand));
                    draw_indirect_array += sizeof(DrawIndexedIndirectCommand);
                }
                batch_draw_indirect_data_offset += sizeof(DrawIndexedIndirectCommand) * num_entity;
                batch_draw_data_offset += draw_data_instance_size * num_entity;
            }
        }

        total_visible_entities += total_entities;
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

    void Renderer::copy_buffers() {
        // Reset staging buffer offset
        uint32_t current_frame = device->get_current_frame();
        ASSERT(current_frame < AppSettings::K_MAX_FRAME_IN_FLIGHTS);

        GPUBufferLinearAllocator *gpu_frame_allocator = &per_frame_allocator[current_frame];
        gpu_frame_allocator->reset();

        // Copy per frame uniform data
        uint32_t per_frame_data_size = align_memory(cast_u32(sizeof(Scene::FrameData)), 64);
        BufferView per_frame_data_buffer = gpu_frame_allocator->allocate(per_frame_data_size);
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
        BufferView cascade_data_buffer = gpu_frame_allocator->allocate(cascade_data_size);
        std::memcpy(cascade_data_buffer.ptr, &shadow_system->cascade_info, cascade_data_size);
        descriptor_info.buffer_info.offset = cascade_data_buffer.offset;
        descriptor_info.buffer_info.size = cascade_data_size;
        cascade_data_descriptor = resource_heap.push_descriptors_per_frame(device.get(), &descriptor_info, 1);

        // Populate per-frame batch data
        total_visible_entities = 0;
        upload_batch_data(main_render_batches, current_frame);
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
        current_frame_index = device->get_current_frame();

        resource_heap.new_frame(current_frame_index);
        line_renderer->new_frame(current_frame_index);

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
            uint32_t render_object_count = scene->render_object_count.load();
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

    void Renderer::patch_global_data(CommandBuffer *command_buffer) {
        if (scene->updated_transforms.size() == 0 && scene->updated_materials.size() == 0)
            return;

        ScopedGpuProfiling(command_buffer, "Patch global buffers");

        GPUBufferLinearAllocator *gpu_frame_allocator = &per_frame_allocator[current_frame_index];

        // Patch transforms
        auto &comp_manager = scene->ecs->component_manager;
        if (scene->updated_transforms.size() > 0) {
            auto &transform_components = comp_manager->get_component_array<TransformComponent>()->components;
            auto &updated_transforms = scene->updated_transforms;

            uint32_t transform_count = cast_u32(updated_transforms.size());
            uint32_t transform_size = transform_count * sizeof(glm::mat4);
            BufferView temp_buffer = gpu_frame_allocator->allocate(transform_size);
            uint8_t *transform_array = temp_buffer.ptr;
            for (uint32_t index : updated_transforms) {
                std::memcpy(transform_array, &transform_components[index].world_transform[0][0], sizeof(glm::mat4));
                transform_array += sizeof(glm::mat4);
            }

            copy_continuous_region(command_buffer, updated_transforms,
                                   BufferView{
                                       temp_buffer.buffer,
                                       temp_buffer.offset,
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

            uint32_t material_instance_size = cast_u32(sizeof(Material3D::Properties));
            uint32_t material_count = cast_u32(updated_materials.size());
            uint32_t material_size = material_count * material_instance_size;
            BufferView temp_buffer = gpu_frame_allocator->allocate(material_size);
            uint8_t *material_array = temp_buffer.ptr;

            for (uint32_t index : updated_materials) {
                std::memcpy(material_array, &materials[index]->properties, material_instance_size);
                material_array += material_instance_size;
            }

            copy_continuous_region(command_buffer, updated_materials,
                                   BufferView{
                                       temp_buffer.buffer,
                                       temp_buffer.offset,
                                       material_size,
                                   },
                                   BufferView{
                                       global_material_buffer,
                                       0,
                                       K_MAX_ENTITIES * material_instance_size,
                                   },
                                   material_instance_size);
        }
    }

    void Renderer::render() {

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();
        cb->begin();

        cb->bind_resource_heap(resource_heap.buffer);
        cb->bind_sampler_heap(sampler_heap.buffer);

        miProfiler::BeginFrame(cb);
        {
            ScopedCpuProfiling("CPU Render Time");
            ScopedGpuProfiling(cb, "GPU Time");
            // Copy per frame data from staging buffer to gpu uniform buffer
            copy_buffers();

            // Generate and Upload visible lights
            upload_visible_lights();

            // Patch transform and Materials if it has changed
            patch_global_data(cb);

            // Update skinned mesh
            update_skinned_mesh(cb);

            // create TLAS
            create_tlas(cb);

            RenderContext context{this, cb};

            frame_graph->execute(&context);

            if (frame_graph->get_present_texture() != K_SWAPCHAIN_TEXTURE_HANDLE)
                cb->copy_to_swapchain(frame_graph->get_present_texture());

            device->queue_command_buffer(cb);
        }

        miProfiler::EndFrame();

        device->present();
    }

    Renderer::~Renderer() {
        vertex_buffer_allocator.destroy();
        index_buffer_allocator.destroy();
        for (uint32_t i = 0; i < AppSettings::K_MAX_FRAME_IN_FLIGHTS; ++i)
            per_frame_allocator[i].destroy();

        if (device->supports_raytracing()) {
            auto &comp_manager = scene->ecs->component_manager;
            auto mesh_comp_ptr = comp_manager->get_component_array<MeshComponent>();
            for (auto &mesh_component : mesh_comp_ptr->components) {
                for (auto &blas : mesh_component.blases)
                    device->destroy_acceleration_structures(&blas.as, 1);
            }

            device->destroy_acceleration_structures(&tlas.as, 1);
            if (blas_buffer_dynamic.is_valid())
                device->destroy_buffers(&blas_buffer_dynamic, 1);
            if (blas_buffer_static.is_valid())
                device->destroy_buffers(&blas_buffer_static, 1);
            if (tlas_buffer.is_valid())
                device->destroy_buffers(&tlas_buffer, 1);
        }

        BufferID buffers[] = {
            global_material_buffer,
            global_transform_buffer,
            resource_heap.buffer,
            sampler_heap.buffer,
        };
        device->destroy_buffers(buffers, cast_u32(std::size(buffers)));
        frame_graph.reset();
        frame_graph_blackboard.reset();
        line_renderer.reset();
        shader_registry_map->destroy();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai