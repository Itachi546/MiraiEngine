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

    SkinningComputePass::SkinningComputePass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<SkinningComputePassData>(
            "SkinningComputePass",
            [board](FrameGraph::Builder &builder, SkinningComputePassData &data) {
                data.output_buffer = builder.add_buffer(Renderer::get()->vertex_buffer_allocator.buffer, "GlobalVertexBuffer");
                builder.write(data.output_buffer, {
                                                      .access_flags = ACCESS_FLAG_SHADER_WRITE | ACCESS_FLAG_SHADER_READ,
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

                if (scene->animation_players.size() == 0)
                    return;

                auto &component_manager = scene->ecs->component_manager;
                auto animator_component_ptr = component_manager->get_component_array<AnimatorComponent>();
                if (animator_component_ptr->components.size() == 0)
                    return;

                ScopedCpuProfiling("ComputeSkinningSetup");

                // Upload skinning matrix
                std::vector<uint32_t> skinned_matrix_offsets(scene->animation_players.size());
                uint32_t skinned_matrix_size = 0;
                for (uint32_t i = 0; i < scene->animation_players.size(); ++i) {
                    std::unique_ptr<AnimationPlayer> &animation_player = scene->animation_players[i];
                    skinned_matrix_offsets[i] = skinned_matrix_size;
                    skinned_matrix_size += cast_u32(animation_player->matrix_palletes.size());
                }

                if (skinned_matrix_size == 0)
                    return;

                GPUBufferLinearAllocator *allocator = renderer->get_per_frame_gpu_allocator();
                BufferView matrix_pallete_buffer = allocator->allocate(cast_u32(skinned_matrix_size * sizeof(glm::mat4)));
                uint8_t *ptr = matrix_pallete_buffer.ptr;
                for (const auto &animation_player : scene->animation_players) {
                    const std::vector<glm::mat4> &matrix_pallete = animation_player->matrix_palletes;
                    uint32_t matrix_pallete_size = cast_u32(sizeof(glm::mat4) * matrix_pallete.size());
                    std::memcpy(ptr, matrix_pallete.data(), matrix_pallete_size);
                    ptr += matrix_pallete_size;
                }

                // List all the animated mesh
                std::vector<SkinnedMeshData> skinned_mesh_data;
                for (auto entity : animator_component_ptr->entities) {
                    MeshComponent *mesh_component = component_manager->get_component<MeshComponent>(entity);
                    ASSERT(mesh_component != nullptr);

                    AnimatorComponent *animator_component = component_manager->get_component<AnimatorComponent>(entity);
                    for (auto &subset : mesh_component->mesh_subsets) {

                        // Vertices is access as uint in the shader, so the offset/stride should be
                        // in the sizeof uint instead of bytes
                        skinned_mesh_data.emplace_back(SkinnedMeshData{
                            .vertex_address = subset.vertex_offset_bytes / 4,
                            .vertex_stride = subset.vertex_stride / 4,
                            .vertex_count = subset.vertex_count,
                            .output_offset = subset.output_vertex_offset_bytes / 4,
                            // Access as mat4 in shader, so we don't convert it to bytes
                            .matrix_palletes_offset = skinned_matrix_offsets[animator_component->animation_player_index],
                        });
                    }
                }

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
                };

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(cast_u32(sizeof(uint32_t) * 8), descriptors, cast_u32(sizeof(descriptors)));

                for (auto &data : skinned_mesh_data) {
                    command_buffer->set_push_data(0, &data, cast_u32(sizeof(data)));
                    command_buffer->dispatch(data.vertex_count, 1, 1);
                }
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai
