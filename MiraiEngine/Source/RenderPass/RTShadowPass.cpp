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

    struct RTVelocityNormalPassData {
        FrameGraphResourceHandle velocity_texture;
        // View space normal vector
        FrameGraphResourceHandle normal_texture;
        std::shared_ptr<ComputeShader> shader;
    };

    RTShadowPass::RTShadowPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
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
                                                                                        .format = FORMAT_R16_SFLOAT,
                                                                                        .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                    });

                builder.write(data.velocity_texture, {
                                                         .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                         .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                         .layout = IMAGE_LAYOUT_GENERAL,
                                                     });

                data.normal_texture = builder.create_texture("RTDepthTexture", {
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

                builder.write(data.normal_texture, {
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

                data.shader = std::make_shared<ComputeShader>("RTVelocityGenShader", "SPIRV/rt-velocity.comp.spv");
            },
            [](const RTVelocityNormalPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                ScopedGpuProfiling(command_buffer, "RTVelocityNormalPass");

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();

                struct PushData {
                    glm::mat4 inv_VP;
                    glm::mat4 prev_VP;
                    glm::vec2 inv_resolution;
                    uint32_t depth_texture_index;
                    uint32_t _padding;
                } push_data;

                push_data.inv_VP = camera->get_inv_view_projection_transform();
                push_data.prev_VP = scene->per_frame_data.prev_VP;
                push_data.inv_resolution = 1.0f / glm::vec2(cast_float(width), cast_float(height));
                push_data.depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id;
                push_data._padding = 0;

                TextureID velocity_texture = pass_resource.get<FrameGraphTexture>(data.velocity_texture).id;
                TextureID normal_texture = pass_resource.get<FrameGraphTexture>(data.normal_texture).id;

                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(velocity_texture, DescriptorType::StorageImage),
                    renderer->get_or_create_descriptor(normal_texture, DescriptorType::StorageImage),
                };

                data.shader->bind(command_buffer);

                uint32_t push_data_size = cast_u32(sizeof(PushData));
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t local_size_x = rendering_utils::get_workgroup_size(width, 32);
                uint32_t local_size_y = rendering_utils::get_workgroup_size(height, 32);
                command_buffer->dispatch(local_size_x, local_size_y, 1);
            });
    }
} // namespace mirai