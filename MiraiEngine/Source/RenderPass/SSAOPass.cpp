#include "SSAOPass.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "Scene/Camera.hpp"
#include "Engine/Profiler.hpp"
#include "Engine/AppSettings.hpp"
#include "Scene/TextureCache.hpp"
namespace mirai {
    struct HBAOConstants {
        glm::mat4 projection_matrix;

        glm::vec2 ssao_texture_resolution;
        glm::vec2 inv_ssao_texture_resolution;

        glm::vec2 inv_noise_texture_resolution;
        float radius_to_screen;
        float neg_inv_r2;

        float num_step;
        float direction_step;
        float intensity;
        float tangent_bias;

        uint32_t noise_texture_index;
        uint32_t view_normal_depth_texture_index;
        uint32_t _padding[2];
    };
    static_assert(sizeof(HBAOConstants) % 4 == 0);

    struct BlurConstants {
        float width;
        float height;
        float blur_direction;
        float blur_radius;

        float sharpness;
        float znear;
        float zfar;
        uint32_t depth_texture_index;

        uint32_t ssao_texture_index;
        uint32_t padding[3];
    };

    struct SSAOBlurData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    const float SSAO_RESOLUTION_SCALE = 0.5f;
    SSAOPass::SSAOPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        // SSAO Pass
        frame_graph->add_callback_pass<SSAOPassData>(
            "SSAOPass",
            [board](FrameGraph::Builder &builder, SSAOPassData &data) {
                const uint32_t width = AppSettings::get_width();
                const uint32_t height = AppSettings::get_height();

                data.output = builder.create_texture("SSAOTexture", {
                                                                        .create_flags = 0,
                                                                        .width = cast_u32(width * SSAO_RESOLUTION_SCALE),
                                                                        .height = cast_u32(height * SSAO_RESOLUTION_SCALE),
                                                                        .depth = 1,
                                                                        .mip_levels = 1,
                                                                        .array_layers = 1,
                                                                        .texture_type = TEXTURE_TYPE_2D,
                                                                        .format = FORMAT_R16_SFLOAT,
                                                                        .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                    });
                builder.write(data.output, {
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

                board->add<SSAOPassData>(data);

                // Create Shader
                data.shader = std::make_shared<ComputeShader>("SSAOPass", "SPIRV/hbao.comp.spv");

                // Load Noise texture
                // SSAO Params
                HBAOParams params = {
                    .noise_texture_inv_dim = 1.0f / 128.0f,
                    .radius = 0.5f,
                    .intensity = 2.0f,
                    .num_directional_step = 4,
                    .num_step = 8,
                    .tangent_bias = 0.1f,
                    .blur_sharpness = 20,
                    .blur_radius = 4,
                };
                board->add<HBAOParams>(std::move(params));
            },
            [](const SSAOPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Camera *camera = renderer->get_scene()->get_camera();

                float screen_width = cast_float(AppSettings::get_width());
                float screen_height = cast_float(AppSettings::get_height());

                float fov = glm::radians(renderer->get_scene()->get_camera()->get_fov());
                float projection_scale = float(screen_height) / (tanf(fov * 0.5f) * 2.0f);

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

                glm::vec2 ssao_resolution{screen_width * SSAO_RESOLUTION_SCALE, screen_height * SSAO_RESOLUTION_SCALE};
                const ViewNormalDepthPassData &view_normal_pass_data = board->get<ViewNormalDepthPassData>();
                const HBAOParams &params = board->get<HBAOParams>();
                HBAOConstants push_constants = {
                    .projection_matrix = camera->get_projection_transform(),
                    .ssao_texture_resolution = ssao_resolution,
                    .inv_ssao_texture_resolution = 1.0f / ssao_resolution,
                    .inv_noise_texture_resolution = glm::vec2{params.noise_texture_inv_dim},
                    .radius_to_screen = params.radius * projection_scale,
                    .neg_inv_r2 = -1.0f / (params.radius * params.radius),
                    .num_step = cast_float(params.num_step),
                    .direction_step = cast_float(params.num_directional_step),
                    .intensity = params.intensity,
                    .tangent_bias = params.tangent_bias,
                    .noise_texture_index = renderer->blue_noise_texture128.id,
                    .view_normal_depth_texture_index = pass_resource.get<FrameGraphTexture>(view_normal_pass_data.output).id.id,
                };

                ScopedGpuProfiling(command_buffer, "SSAOPass");
                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
                };

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(HBAOConstants));
                command_buffer->set_push_data(sizeof(HBAOConstants), descriptors, cast_u32(sizeof(descriptors)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(screen_width * SSAO_RESOLUTION_SCALE), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(screen_height * SSAO_RESOLUTION_SCALE), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });

        // SSAO Horizontal Blur Pass
        frame_graph->add_callback_pass<SSAOBlurData>(
            "SSAOHorizontalBlurPass",
            [board](FrameGraph::Builder &builder, SSAOBlurData &data) {
                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                data.output = builder.create_texture("SSAOBlurTexture", {
                                                                            .create_flags = 0,
                                                                            .width = cast_u32(width * SSAO_RESOLUTION_SCALE),
                                                                            .height = cast_u32(height * SSAO_RESOLUTION_SCALE),
                                                                            .depth = 1,
                                                                            .mip_levels = 1,
                                                                            .array_layers = 1,
                                                                            .texture_type = TEXTURE_TYPE_2D,
                                                                            .format = FORMAT_R16_SFLOAT,
                                                                            .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                        });
                builder.write(data.output, {
                                               .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                               .layout = IMAGE_LAYOUT_GENERAL,
                                           });

                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                builder.read(ssao_pass_data.output, {
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

                // Create Shader
                data.shader = std::make_shared<ComputeShader>("SSAOBlur", "SPIRV/ssao-blur.comp.spv");
                board->add<SSAOBlurData>(data);
            },
            [](const SSAOBlurData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Camera *camera = renderer->get_scene()->get_camera();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                const HBAOParams &params = board->get<HBAOParams>();
                const SSAOPassData &ssao_data = board->get<SSAOPassData>();
                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();
                BlurConstants push_constants = {
                    .width = width * SSAO_RESOLUTION_SCALE,
                    .height = height * SSAO_RESOLUTION_SCALE,
                    .blur_direction = 0,
                    .blur_radius = params.blur_radius,
                    .sharpness = params.blur_sharpness,
                    .znear = camera->get_near_plane(),
                    .zfar = camera->get_far_plane(),
                    .depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id,
                    .ssao_texture_index = pass_resource.get<FrameGraphTexture>(ssao_data.output).id.id,
                };

                ScopedGpuProfiling(command_buffer, "SSAOHorizontalBlurPass");
                DescriptorOffset bindings[] = {
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
                };

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOHorizontalBlurPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(BlurConstants));
                command_buffer->set_push_data(sizeof(BlurConstants), &bindings, cast_u32(sizeof(bindings)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(width * SSAO_RESOLUTION_SCALE), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(height * SSAO_RESOLUTION_SCALE), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });

        // SSAO Vertical Blur Pass
        frame_graph->add_callback_pass(
            "SSAOVerticalBlurPass",
            [board](FrameGraph::Builder &builder, FrameGraph::NoData &) {
                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                const SSAOBlurData &hblur_data = board->get<SSAOBlurData>();

                builder.write(ssao_pass_data.output, {
                                                         .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                         .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                         .layout = IMAGE_LAYOUT_GENERAL,
                                                     });
                builder.read(hblur_data.output, {
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
            },
            [](const FrameGraph::NoData &, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Camera *camera = renderer->get_scene()->get_camera();

                uint32_t width = AppSettings::get_width();
                uint32_t height = AppSettings::get_height();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const HBAOParams &params = board->get<HBAOParams>();
                const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
                const SSAOBlurData &data = board->get<SSAOBlurData>();

                BlurConstants push_constants = {
                    .width = width * SSAO_RESOLUTION_SCALE,
                    .height = height * SSAO_RESOLUTION_SCALE,
                    .blur_direction = 1,
                    .blur_radius = params.blur_radius,
                    .sharpness = params.blur_sharpness,
                    .znear = camera->get_near_plane(),
                    .zfar = camera->get_far_plane(),
                    .depth_texture_index = pass_resource.get<FrameGraphTexture>(depth_prepass_data.output).id.id,
                    .ssao_texture_index = pass_resource.get<FrameGraphTexture>(data.output).id.id,
                };

                ScopedGpuProfiling(command_buffer, "SSAOVerticalBlurPass");

                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                DescriptorOffset bindings[] = {
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(ssao_pass_data.output).id, DescriptorType::StorageImage),
                };

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOVerticalPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(BlurConstants));
                command_buffer->set_push_data(sizeof(BlurConstants), bindings, cast_u32(sizeof(bindings)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(width * SSAO_RESOLUTION_SCALE), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(height * SSAO_RESOLUTION_SCALE), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai