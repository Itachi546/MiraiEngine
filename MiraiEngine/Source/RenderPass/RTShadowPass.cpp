#include "RTShadowPass.hpp"

#include "Engine/Profiler.hpp"
#include "Graphics/RenderingDevice.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Camera.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/AppSettings.hpp"

namespace mirai {

    struct RTShadowVisibilityPassData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    RTShadowPass::RTShadowPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<RTShadowVisibilityPassData>(
            "RTVisibilityPass",
            [board](FrameGraph::Builder &builder, RTShadowVisibilityPassData &data) {
                uint32_t width = AppSettings::get_width() / 2;
                uint32_t height = AppSettings::get_height() / 2;

                // @TODO if we store only bit value for a texture, we can further optimize it
                data.output = builder.create_texture("RTVisibilityTexture", {
                                                                                .create_flags = 0,
                                                                                .width = width,
                                                                                .height = height,
                                                                                .depth = 1,
                                                                                .mip_levels = 1,
                                                                                .array_layers = 1,
                                                                                .texture_type = TEXTURE_TYPE_2D,
                                                                                .format = FORMAT_R8_UNORM,
                                                                                .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                            });

                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_SHADER_READ,
                                                            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                            .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                        });

                data.shader = std::make_shared<ComputeShader>("RTVelocityGenShader", "SPIRV/rt-shadow-visibility.comp.spv");
                builder.set_side_effect();
            },
            [](const RTShadowVisibilityPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                ScopedGpuProfiling(command_buffer, "RTVisibilityPass");

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                uint32_t width = AppSettings::get_width() / 2;
                uint32_t height = AppSettings::get_height() / 2;

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                struct PushData {
                    glm::mat4 inv_VP;

                    glm::vec2 resolution;
                    glm::vec2 inv_resolution;

                    glm::vec3 direction_or_position;
                    float cos_angular_radius;

                    uint32_t depth_texture_index;
                    uint32_t light_type;
                    uint32_t noise_texture_index;
                    uint32_t frame_index;

                } push_data;

                Entity sun = scene->get_default_directional_light();
                TransformComponent *transform = scene->ecs->component_manager->get_component<TransformComponent>(sun);
                LightComponent *light = scene->ecs->component_manager->get_component<LightComponent>(sun);

                glm::vec2 resolution = glm::vec2(cast_float(width), cast_float(height));
                push_data.inv_VP = camera->get_inv_view_projection_transform();
                push_data.resolution = resolution;
                push_data.inv_resolution = 1.0f / resolution;

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                push_data.depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id;
                push_data.light_type = cast_u32(LIGHT_TYPE_DIRECTIONAL);
                push_data.noise_texture_index = renderer->blue_noise_texture128.id;

                // Angular radius of sun
                push_data.cos_angular_radius = cos(light->radius);
                push_data.direction_or_position = quat_to_direction(transform->rotation);
                push_data.frame_index = cast_u32(renderer->frame_id & UINT32_MAX);

                TextureID visibility_texture = pass_resource.get<FrameGraphTexture>(data.output).id;
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(visibility_texture, DescriptorType::StorageImage),
                    renderer->get_or_create_descriptor(renderer->tlas.as, DescriptorType::AccelerationStructure),
                };

                data.shader->bind(command_buffer);

                uint32_t push_data_size = cast_u32(sizeof(PushData));
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, 8);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, 8);
                command_buffer->dispatch(local_size_x, local_size_y, 1);

                // @TODO temp
                command_buffer->prepare_image_for_shader_read(visibility_texture);
            });

        /*
        frame_graph->add_callback_pass<RTVelocityNormalPassData>(
            "RTVelocityPass",
            [board](FrameGraph::Builder &builder, RTVelocityNormalPassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                data.velocity_texture = builder.create_texture("RTVelocityTexture", {
                                                                                        .create_flags = 0,
                                                                                        .width = width,
                                                                                        .height = height,
                                                                                        .depth = 1,
                                                                                        .mip_levels = 1,
                                                                                        .array_layers = 1,
                                                                                        .texture_type = TEXTURE_TYPE_2D,
                                                                                        .format = FORMAT_R16G16_SFLOAT,
                                                                                        .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                    });

                builder.write(data.velocity_texture, {
                                                         .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                         .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                         .layout = IMAGE_LAYOUT_GENERAL,
                                                     });

                const ViewNormalDepthPassData &view_pass_data = board->get<ViewNormalDepthPassData>();
                builder.read(view_pass_data.output, {
                                                        .access_flags = ACCESS_FLAG_SHADER_READ,
                                                        .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                        .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                    });

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_SHADER_READ,
                                                            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                            .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                        });

                data.shader = std::make_shared<ComputeShader>("RTVelocityGenShader", "SPIRV/rt-velocity.comp.spv");
            },
            [](const RTVelocityNormalPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                ScopedGpuProfiling(command_buffer, "RTVelocityNormalPass");

                const auto &resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                struct PushData {
                    glm::mat4 inv_VP;
                    glm::mat4 prev_VP;

                    glm::vec2 resolution;
                    glm::vec2 inv_resolution;

                    uint32_t depth_texture_index;
                    uint32_t view_normal_depth_texture_index;
                    uint32_t _padding[2];
                } push_data;

                glm::vec2 resolution = glm::vec2(cast_float(width), cast_float(height));
                push_data.inv_VP = camera->get_inv_view_projection_transform();
                push_data.prev_VP = scene->per_frame_data.prev_VP;
                push_data.resolution = resolution;
                push_data.inv_resolution = 1.0f / resolution;

                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                push_data.depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id;

                const ViewNormalDepthPassData &view_data_pass = board->get<ViewNormalDepthPassData>();
                push_data.view_normal_depth_texture_index = pass_resource.get<FrameGraphTexture>(view_data_pass.output).id.id;

                TextureID velocity_texture = pass_resource.get<FrameGraphTexture>(data.velocity_texture).id;
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(velocity_texture, DescriptorType::StorageImage),
                };

                data.shader->bind(command_buffer);

                uint32_t push_data_size = cast_u32(sizeof(PushData));
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, 32);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, 32);
                command_buffer->dispatch(local_size_x, local_size_y, 1);
            });
            */
    }
} // namespace mirai