#include "RTGroundTruthPass.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/Camera.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    RTGroundTruthPass::RTGroundTruthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<RTGroundTruthPassData>(
            "RTGroundTruthPass",
            [board](FrameGraph::Builder &builder, RTGroundTruthPassData &data) {
                data.shader = std::make_shared<RTShader>("RTGroundTruthShader",
                                                         "SPIRV/gt-path-trace.rgen.spv",
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rchit.spv"},
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rmiss.spv"});
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                data.output = builder.create_texture("RTGroundTruthOutput", {
                                                                                .create_flags = 0,
                                                                                .width = width,
                                                                                .height = height,
                                                                                .depth = 1,
                                                                                .mip_levels = 1,
                                                                                .array_layers = 1,
                                                                                .texture_type = TEXTURE_TYPE_2D,
                                                                                .format = FORMAT_B8G8R8A8_UNORM,
                                                                                .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                                                                            });
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                               .stage_mask = PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });
                board->add<RTGroundTruthPassData>(data);
            },
            [](const RTGroundTruthPassData &data, const FrameGraphPassResource &pass_resources, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;
                Renderer *renderer = ctx->renderer;

                if (!renderer->tlas.as.is_valid())
                    return;

                ScopedGpuProfiling(command_buffer, "RTGroundTruthPass");
                command_buffer->begin_gpu_debug_label("RTGroundTruth Pass");

                const auto &resource_states = pass_resources.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                struct PushData {
                    glm::mat4 inv_P;
                    glm::mat4 inv_V;
                    glm::vec3 camera_position;
                    uint32_t skybox_texture_index;
                } push_data;

                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();
                push_data.inv_P = camera->get_inv_projection_transform();
                push_data.inv_V = camera->get_inv_view_transform();
                push_data.camera_position = glm::vec4(camera->position, 1.0f);
                push_data.skybox_texture_index = scene->get_environment_map()->get_cubemap().id;

                uint32_t push_data_size = cast_u32(sizeof(PushData));

                // @TODO we can use cached descriptor later
                BufferID buffer = renderer->geometry_buffer_allocator->allocations[0].id;
                DescriptorInfo descriptor_info = {.type = DescriptorType::StorageBuffer, .resource = buffer, .buffer_info = {0, UINT64_MAX}};
                DescriptorOffset geometry_descriptor = renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &descriptor_info, 1);
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(renderer->tlas.as, DescriptorType::AccelerationStructure),
                    renderer->get_or_create_descriptor(pass_resources.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
                    renderer->rt_instance_data_descriptor,
                    geometry_descriptor,
                    renderer->material_descriptor,
                };

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                command_buffer->trace_rays(width, height);

                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai