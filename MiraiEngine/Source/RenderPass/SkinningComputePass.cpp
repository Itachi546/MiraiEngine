#include "SkinningComputePass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "Common/JobSystem.hpp"

namespace mirai {
    struct SkinnedMeshData {
        uint32_t vertex_address;
        uint32_t vertex_stride;
        uint32_t vertex_count;
        uint32_t output_offset;

        uint32_t matrix_palletes_offset;
        uint32_t _padding[3];
    };

    const uint32_t K_DEFAULT_SKINNED_BUFFER_SIZE = 8 * 1024 * 1024; // 8mb
    const uint32_t K_OUTPUT_VERTEX_DATA_SIZE = 32;

    SkinningComputePass::SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<SkinningComputePassData>(
            "SkinningComputePass",
            [board](FrameGraph::Builder &builder, SkinningComputePassData &data) {
                data.output_buffer = builder.create_buffer("SkinnedOutputBuffer", {
                                                                                      .size = K_DEFAULT_SKINNED_BUFFER_SIZE,
                                                                                      .usage_flags = BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                                                                      .allocation_type = MEMORY_ALLOCATION_TYPE_GPU,
                                                                                  });
                builder.write(data.output_buffer, {
                                                      .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                      .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                  });

                data.shader = std::make_shared<ComputeShader>("SkinningCS", "SPIRV/skinning.comp.spv");
                board->add<SkinningComputePassData>(data);
            },

            [](const SkinningComputePassData &data, const FrameGraphPassResource &pass_resources, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                Scene *scene = renderer->get_scene();
                CommandBuffer *command_buffer = ctx->command_buffer;

                ScopedCpuProfiling("ComputeSkinningSetup");
                /*
                std::vector<SkinnedMeshData> skinned_mesh_data;
                auto &component_manager = scene->ecs->component_manager;
                auto animator_component_ptr = component_manager->get_component_array<AnimatorComponent>();

                // Generate skeleton renderdata
                uint32_t output_offset_bytes = 0;
                for (auto entity : animator_component_ptr->entities) {
                    AnimatorComponent *animator_component = component_manager->get_component<AnimatorComponent>(entity);
                    MeshComponent *mesh_component = component_manager->get_component<MeshComponent>(entity);
                    ASSERT(mesh_component != nullptr);
                    for (auto &subset : mesh_component->mesh_subsets) {
                        uint32_t skeleton_index = animator_component->skeleton_index;
                        Skeleton *skeleton = &scene->skeletons[skeleton_index];

                        skinned_mesh_data.emplace_back(SkinnedMeshData{
                            .vertex_address = subset.vertex_offset_bytes,
                            .vertex_stride = subset.vertex_stride,
                            .vertex_count = subset.vertex_count,
                            .output_offset = output_offset_bytes,
                            .matrix_palletes_offset = skeleton_pallete_offset[skeleton_index],
                        });
                        output_offset_bytes += K_OUTPUT_VERTEX_DATA_SIZE * subset.vertex_count;
                    }
                }

                ASSERT(output_offset_bytes < K_DEFAULT_SKINNED_BUFFER_SIZE);

                ScopedGpuProfiling(command_buffer, "SkinningCS");

                command_buffer->prepare_resources(pass_resources.get_resource_access_states());
                command_buffer->begin_gpu_debug_label("SkinningCS");

                DescriptorInfo matrix_pallete_descriptor_info = {
                    .type = DescriptorType::StorageBuffer,
                    .resource = matrix_pallete_buffer.buffer,
                    .buffer_info = {
                        .offset = matrix_pallete_buffer.offset,
                        .size = matrix_pallete_buffer.size,
                    }};

                DescriptorOffset descriptors[] = {
                    renderer->global_geometry_descriptor,
                    renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &matrix_pallete_descriptor_info, 1),
                    renderer->get_or_create_descriptor(pass_resources.get<FrameGraphBuffer>(data.output_buffer).id, DescriptorType::StorageBuffer),
                };

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(cast_u32(sizeof(uint32_t) * 8), descriptors, cast_u32(sizeof(descriptors)));

                for (auto &data : skinned_mesh_data) {
                    command_buffer->set_push_data(0, &data, cast_u32(sizeof(data)));
                    command_buffer->dispatch(data.vertex_count, 1, 1);
                }
                command_buffer->end_gpu_debug_label();
                */
            });
    }
} // namespace mirai
