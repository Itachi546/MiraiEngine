#include "ViewNormalDepthPass.hpp"

#include "Common/Profiler.hpp"
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

    ViewNormalDepthPass::ViewNormalDepthPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<ViewNormalDepthPassData>(
            "ViewNormalDepthPass",
            [board](FrameGraph::Builder &builder, ViewNormalDepthPassData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                data.output = builder.create_texture("ViewNormalDepthTexture", {

                                                                                   .create_flags = 0,
                                                                                   .width = width,
                                                                                   .height = height,
                                                                                   .depth = 1,
                                                                                   .mip_levels = 1,
                                                                                   .array_layers = 1,
                                                                                   .texture_type = TEXTURE_TYPE_2D,
                                                                                   .format = FORMAT_R16G16B16A16_SFLOAT,
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

                data.shader = std::make_shared<ComputeShader>("ViewNormalDepthShader", "SPIRV/view-normal-depth.comp.spv");

                board->add<ViewNormalDepthPassData>(data);
            },
            [](const ViewNormalDepthPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Scene *scene = renderer->get_scene();
                Camera *camera = scene->get_camera();

                ScopedGpuProfiling(command_buffer, "ViewNormalDepthPass");

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();

                struct PushData {
                    glm::mat4 inverse_projection_matrix;
                    glm::vec2 inv_resolution;
                    uint32_t depth_texture_index;
                    uint32_t _padding;
                } push_data;

                push_data.inverse_projection_matrix = camera->get_inv_projection_transform();
                push_data.inv_resolution = 1.0f / glm::vec2(cast_float(width), cast_float(height));
                push_data.depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id;
                push_data._padding = 0;

                TextureID view_normal_depth_texture = pass_resource.get<FrameGraphTexture>(data.output).id;
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(view_normal_depth_texture, DescriptorType::StorageImage),
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