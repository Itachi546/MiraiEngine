#include "RTGroundTruthPass.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/Camera.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"
#include "Common/Profiler.hpp"
#include "Scene/TextureCache.hpp"
namespace mirai {

    uint32_t current_texture_index = 0;
    uint32_t frame_count = 0;
    glm::vec3 last_cam_position = glm::vec3(0.0f);
    glm::vec3 last_cam_rotation = glm::vec3(0.0f);

    RTGroundTruthPass::RTGroundTruthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<RTGroundTruthPassData>(
            "RTGroundTruthPass",
            [board](FrameGraph::Builder &builder, RTGroundTruthPassData &data) {
                data.shader = std::make_shared<RTShader>("RTGroundTruthShader",
                                                         "SPIRV/gt-path-trace.rgen.spv",
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rchit.spv"},
                                                         std::vector<std::string>{"SPIRV/gt-path-trace.rmiss.spv"}, 4);
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                TextureDescription texture_desc = {
                    .create_flags = 0,
                    .width = width,
                    .height = height,
                    .depth = 1,
                    .mip_levels = 1,
                    .array_layers = 1,
                    .texture_type = TEXTURE_TYPE_2D,
                    .format = FORMAT_R16G16B16A16_SFLOAT,
                    .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                };

                RenderingDevice *device = RenderingDevice::get();
                for (int i = 0; i < 2; ++i) {
                    std::string name = "RTGroundTruthTexture" + std::to_string(i);
                    data.color_textures[i] = device->create_texture(&texture_desc, name);
                    TextureCache::get()->add_texture(name, data.color_textures[i]);
                    Renderer::get()->add_bindless_texture(data.color_textures[i]);
                }

                data.output = builder.add_texture(data.color_textures[current_texture_index], "RTGroundTruthTexture");

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
                RenderDebugData &debug_data = renderer->get_frame_graph_blackboard()->get<RenderDebugData>();

                if (!renderer->tlas.as.is_valid() || !debug_data.show_rt_ground_truth) {
                    frame_count = 0;
                    return;
                }

                // @Note hack, update the current output texture
                TextureID write_texture = data.color_textures[current_texture_index];
                renderer->get_frame_graph()->get<FrameGraphTexture>(data.output).id = write_texture;

                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                if (frame_count > 0) {
                    const float EPSILON = 0.1f;
                    float pos_delta = glm::distance2(camera->position, last_cam_position);
                    bool position_changed = pos_delta > EPSILON * 0.01;

                    // Euler angles
                    float rot_delta = glm::distance2(camera->rotation, last_cam_rotation);
                    bool rotation_changed = rot_delta > 0.05f;

                    if (position_changed || rotation_changed || debug_data.reset_rt_texture)
                        frame_count = 0;
                    debug_data.reset_rt_texture = false;
                }

                last_cam_position = camera->position;
                last_cam_rotation = camera->rotation;

                ScopedGpuProfiling(command_buffer, "RTGroundTruthPass");
                command_buffer->begin_gpu_debug_label("RTGroundTruth Pass");

                const auto &resource_states = pass_resources.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                struct PushData {
                    glm::mat4 inv_P;
                    glm::mat4 inv_V;
                    glm::vec3 camera_position;
                    uint32_t skybox_texture_index;

                    uint32_t frame_count;
                    uint32_t input_texture_index;
                    uint32_t directional_light_index;
                    uint32_t _padding;
                } push_data;

                push_data.inv_P = camera->get_inv_projection_transform();
                push_data.inv_V = camera->get_inv_view_transform();
                push_data.camera_position = glm::vec4(camera->position, 1.0f);
                push_data.skybox_texture_index = scene->get_environment_map()->get_cubemap().id;
                push_data.frame_count = frame_count;

                TextureID read_texture = data.color_textures[1 - current_texture_index];
                push_data.input_texture_index = read_texture.id;

                Entity sun = scene->get_default_directional_light();
                push_data.directional_light_index = scene->ecs->component_manager->get_component_index<LightComponent>(sun);

                uint32_t push_data_size = cast_u32(sizeof(PushData));

                // @TODO we can use cached descriptor later
                BufferID buffer = renderer->geometry_buffer_allocator->allocations[0].id;
                DescriptorInfo descriptor_info = {.type = DescriptorType::StorageBuffer, .resource = buffer, .buffer_info = {0, UINT64_MAX}};
                DescriptorOffset geometry_descriptor = renderer->resource_heap.push_descriptors_per_frame(RenderingDevice::get(), &descriptor_info, 1);
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(renderer->tlas.as, DescriptorType::AccelerationStructure),
                    renderer->get_or_create_descriptor(write_texture, DescriptorType::StorageImage),
                    renderer->rt_instance_data_descriptor,
                    geometry_descriptor,
                    renderer->material_descriptor,
                    renderer->light_descriptor,
                };

                TextureBarrierInfo barrier_infos[] = {
                    {
                        .texture_id = read_texture,
                        .stage_mask = PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        .access_mask = ACCESS_FLAG_SHADER_READ,
                        .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    },
                };

                command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                command_buffer->trace_rays(width, height);

                command_buffer->end_gpu_debug_label();

                frame_count++;
                current_texture_index = 1 - current_texture_index;
            });
    }
} // namespace mirai