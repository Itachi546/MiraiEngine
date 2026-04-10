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
        glm::mat4 inv_projection_matrix;

        glm::vec2 ssao_texture_resolution;
        glm::vec2 depth_texture_resolution;

        glm::vec2 inv_depth_texture_resolution;
        glm::vec2 inv_noise_texture_resolution;

        float radius_to_screen;
        float neg_inv_r2;
        float num_step;
        float direction_step;

        float intensity;
        float tangent_bias;
        uint32_t noise_texture_index;
        uint32_t padding;
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
        float _padding;
    };

    struct SSAOBlurBindings {
        uint32_t hblur_bindings[3];
        uint32_t vblur_bindings[3];
    };

    struct HBAOBindings {
        uint32_t descriptors[2];
    };

    struct SSAOBlurData {
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    const float SSAO_WIDTH = AppSettings::default_window_width * AppSettings::resolution_scale;
    const float SSAO_HEIGHT = AppSettings::default_window_height * AppSettings::resolution_scale;

    SSAOPass::SSAOPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        // SSAO Pass
        frame_graph->add_callback_pass<SSAOPassData>(
            "SSAOPass",
            [board](FrameGraph::FrameGraphBuilder &builder, SSAOPassData &data) {
                RenderingDevice *device = RenderingDevice::get();
                data.output = builder.create_texture("SSAOTexture", {
                                                                        .create_flags = 0,
                                                                        .width = cast_u32(SSAO_WIDTH),
                                                                        .height = cast_u32(SSAO_HEIGHT),
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

                DepthPrePassData depth_prepass_data = board->get<DepthPrePassData>();
                builder.read(depth_prepass_data.output, {
                                                            .access_flags = ACCESS_FLAG_SHADER_READ,
                                                            .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                            .layout = IMAGE_LAYOUT_GENERAL,
                                                        });
                data.depth_texture = depth_prepass_data.output;
                board->add<SSAOPassData>(data);

                // Create Shader
                data.shader = std::make_shared<ComputeShader>("SSAOPass", "SPIRV/hbao.comp.spv");

                // Load Noise texture
                TextureID noise_texture = rendering_utils::load_texture2d_from_path("Assets/Textures/blue-noise-128.png");
                // Pass the lifetime management to texture cache
                TextureCache::get()->add_texture("noise-texture-128", noise_texture);
                Renderer::get()->add_bindless_texture(noise_texture);

                // SSAO Params
                HBAOParams params = {
                    .noise_texture = noise_texture,
                    .noise_texture_inv_dim = 1.0f / 128.0f,
                    .radius = 0.5f,
                    .intensity = 2.0f,
                    .num_directional_step = 4,
                    .num_step = 8,
                    .tangent_bias = 0.1f,
                    .blur_sharpness = 40,
                    .blur_radius = 10,
                };
                board->add<HBAOParams>(std::move(params));
            },
            [](const SSAOPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Camera *camera = renderer->get_scene()->get_camera();

                float screen_width = AppSettings::default_window_width * AppSettings::resolution_scale;
                float screen_height = AppSettings::default_window_height * AppSettings::resolution_scale;

                float fov = glm::radians(renderer->get_scene()->get_camera()->get_fov());
                float projection_scale = float(screen_height) / (tanf(fov * 0.5f) * 2.0f);

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const HBAOParams &params = board->get<HBAOParams>();
                HBAOConstants push_constants = {
                    .inv_projection_matrix = camera->get_inv_projection_transform(),
                    .ssao_texture_resolution = {SSAO_WIDTH, SSAO_HEIGHT},
                    .depth_texture_resolution = {screen_width, screen_height},
                    .inv_depth_texture_resolution = {1.0f / screen_width, 1.0f / screen_height},
                    .inv_noise_texture_resolution = glm::vec2{params.noise_texture_inv_dim},
                    .radius_to_screen = params.radius * projection_scale,
                    .neg_inv_r2 = -1.0f / (params.radius * params.radius),
                    .num_step = cast_float(params.num_step),
                    .direction_step = cast_float(params.num_directional_step),
                    .intensity = params.intensity,
                    .tangent_bias = params.tangent_bias,
                    .noise_texture_index = params.noise_texture,
                    .padding = 0,
                };

                HBAOBindings *bindings = nullptr;
                if (!board->has<HBAOBindings>()) {
                    DescriptorInfo descriptor_infos[] = {
                        {.type = DescriptorType::StorageImage, .resource = pass_resource.get<FrameGraphTexture>(data.output).id},
                        {.type = DescriptorType::SampledImage, .resource = pass_resource.get<FrameGraphTexture>(data.depth_texture).id},
                    };
                    DescriptorOffset base_descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
                    bindings = &board->add<HBAOBindings>(HBAOBindings{
                        .descriptors = {base_descriptor_offset, base_descriptor_offset + 1},
                    });
                } else {
                    bindings = &board->get<HBAOBindings>();
                }
                ASSERT(bindings != nullptr);

                ScopedGpuProfiling(command_buffer, "SSAOPass");

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(HBAOConstants));
                command_buffer->set_push_data(sizeof(HBAOConstants), &bindings->descriptors, cast_u32(sizeof(bindings->descriptors)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(SSAO_WIDTH), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(SSAO_HEIGHT), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });

        // SSAO Horizontal Blur Pass
        frame_graph->add_callback_pass<SSAOBlurData>(
            "SSAOHorizontalBlurPass",
            [board](FrameGraph::FrameGraphBuilder &builder, SSAOBlurData &data) {
                RenderingDevice *device = RenderingDevice::get();

                data.output = builder.create_texture("SSAOBlurTexture", {
                                                                            .create_flags = 0,
                                                                            .width = cast_u32(SSAO_WIDTH),
                                                                            .height = cast_u32(SSAO_HEIGHT),
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
                builder.read(ssao_pass_data.depth_texture, {
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

                BlurConstants push_constants = {
                    .width = SSAO_WIDTH,
                    .height = SSAO_HEIGHT,
                    .blur_direction = 0,
                    .blur_radius = params.blur_radius,
                    .sharpness = params.blur_sharpness,
                    .znear = camera->get_near_plane(),
                    .zfar = camera->get_far_plane(),
                    ._padding = 0,
                };

                SSAOBlurBindings *bindings = nullptr;
                if (!board->has<SSAOBlurBindings>()) {
                    // We initialize bindings for both direction in same pass
                    DescriptorInfo descriptor_infos[] = {
                        {.type = DescriptorType::StorageImage, .resource = pass_resource.get<FrameGraphTexture>(data.output).id},
                        {.type = DescriptorType::SampledImage, .resource = pass_resource.get<FrameGraphTexture>(ssao_pass_data.depth_texture).id},
                        {.type = DescriptorType::SampledImage, .resource = pass_resource.get<FrameGraphTexture>(ssao_pass_data.output).id},
                    };
                    DescriptorOffset hblur_descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));

                    descriptor_infos[0].resource = pass_resource.get<FrameGraphTexture>(ssao_pass_data.output).id;
                    descriptor_infos[1].resource = pass_resource.get<FrameGraphTexture>(data.output).id;
                    DescriptorOffset vblur_descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)) - 1);

                    bindings = &board->add<SSAOBlurBindings>(SSAOBlurBindings{
                        .hblur_bindings = {hblur_descriptor_offset, hblur_descriptor_offset + 1, hblur_descriptor_offset + 2},
                        .vblur_bindings = {vblur_descriptor_offset, hblur_descriptor_offset + 1, vblur_descriptor_offset + 1},
                    });
                } else {
                    bindings = &board->get<SSAOBlurBindings>();
                }
                ASSERT(bindings != nullptr);

                ScopedGpuProfiling(command_buffer, "SSAOHorizontalBlurPass");

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOHorizontalBlurPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(BlurConstants));
                command_buffer->set_push_data(sizeof(BlurConstants), &bindings->hblur_bindings, cast_u32(sizeof(bindings->hblur_bindings)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(SSAO_WIDTH), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(SSAO_HEIGHT), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });

        // SSAO Vertical Blur Pass
        frame_graph->add_callback_pass(
            "SSAOVerticalBlurPass",
            [board](FrameGraph::FrameGraphBuilder &builder, FrameGraph::NoData &no_data) {
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
                builder.read(ssao_pass_data.depth_texture, {
                                                               .access_flags = ACCESS_FLAG_SHADER_READ,
                                                               .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                               .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                           });
            },
            [](const FrameGraph::NoData &no_data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;
                Camera *camera = renderer->get_scene()->get_camera();

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const SSAOPassData &ssao_pass_data = board->get<SSAOPassData>();
                const HBAOParams &params = board->get<HBAOParams>();
                BlurConstants push_constants = {
                    .width = SSAO_WIDTH,
                    .height = SSAO_HEIGHT,
                    .blur_direction = 1,
                    .blur_radius = params.blur_radius,
                    .sharpness = params.blur_sharpness,
                    .znear = camera->get_near_plane(),
                    .zfar = camera->get_far_plane(),
                    ._padding = 0,
                };

                SSAOBlurBindings *bindings = &board->get<SSAOBlurBindings>();
                ASSERT(bindings != nullptr);

                const SSAOBlurData &data = board->get<SSAOBlurData>();

                ScopedGpuProfiling(command_buffer, "SSAOVerticalBlurPass");

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);

                command_buffer->begin_gpu_debug_label("SSAOVerticalPass");
                data.shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constants, sizeof(BlurConstants));
                command_buffer->set_push_data(sizeof(BlurConstants), &bindings->vblur_bindings, cast_u32(sizeof(bindings->vblur_bindings)));

                uint32_t work_size_x = rendering_utils::get_workgroup_size(cast_u32(SSAO_WIDTH), 32);
                uint32_t work_size_y = rendering_utils::get_workgroup_size(cast_u32(SSAO_HEIGHT), 32);
                command_buffer->dispatch(work_size_x, work_size_y, 1);
                command_buffer->end_gpu_debug_label();
            });
    }
} // namespace mirai

// #include "SSAOPass.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Engine/Profiler.hpp"
// #include "Scene/Camera.hpp"
// #include "Graphics/Renderer.hpp"
// #include "Scene/Shader.hpp"
// #include "Device/Window.hpp"

// namespace mirai {

//     void SSAOPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         ssao_shader = Shader::create_from_file("SPIRV/hbao.comp.spv", "hbao-shader");
//         blur_shader = Shader::create_from_file("SPIRV/cross-bilateral-blur.comp.spv", "bilateral-blur-shader");

//         ASSERT(node->inputs.size() == 1);
//         ASSERT(node->outputs.size() == 1);
//         FrameGraphResource *depth_resource = frame_graph->get_resource(node->inputs[0]);
//         FrameGraphResource *ssao_resource = frame_graph->get_resource(node->outputs[0]);

//         noise_texture = rendering_utils::load_texture2d_from_path("Assets/Textures/blue-noise-128.png");

//         TextureDescription texture_desc = {
//             .create_flags = 0,
//             .width = ssao_resource->resource_info.width,
//             .height = ssao_resource->resource_info.height,
//             .depth = 1,
//             .mip_levels = 1,
//             .array_layers = 1,
//             .texture_type = TEXTURE_TYPE_2D,
//             .format = FORMAT_R16_SFLOAT,
//             .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
//         };

//         blur_intermediate_texture = device->create_texture(&texture_desc, "blur_intermediate_texture");

//         UniformLayout layouts[] = {
//             {0, BINDING_TYPE_STORAGE_IMAGE, SHADER_STAGE_COMPUTE},
//             {1, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//             {2, BINDING_TYPE_COMBINED_IMAGE_SAMPLER, SHADER_STAGE_COMPUTE},
//         };

//         SamplerDescription sampler_desc = SamplerDescription::create();
//         SamplerID default_sampler = device->create_sampler(&sampler_desc);

//         sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_NEAREST;
//         SamplerID depth_sampler = device->create_sampler(&sampler_desc);

//         sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_LINEAR;
//         sampler_desc.address_mode_u = sampler_desc.address_mode_v = sampler_desc.address_mode_w = SAMPLER_ADDRESS_MODE_REPEAT;
//         SamplerID noise_sampler = device->create_sampler(&sampler_desc);

//         UniformBinding bindings[] = {
//             {.resource_id = ssao_resource->handle},
//             {.resource_id = depth_resource->handle, .texture_info = {.sampler = depth_sampler}},
//             {.resource_id = noise_texture, .texture_info = {.sampler = noise_sampler}},
//         };
//         // SSAO Uniform Set
//         ssao_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "ssao_uniform_set");
//         device->update_uniform_set(ssao_set, bindings, cast_u32(std::size(bindings)));

//         // Create BlurX Uniform Set
//         blur_x_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "blur_x_set");
//         bindings[0].resource_id = blur_intermediate_texture;
//         bindings[2].resource_id = ssao_resource->handle;
//         // Use linear, clamp to edge sampler for blurring
//         bindings[2].texture_info.sampler = default_sampler;
//         device->update_uniform_set(blur_x_set, bindings, cast_u32(std::size(bindings)));

//         // Create BlurY Uniform Set
//         blur_y_set = device->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0, "blur_y_set");
//         bindings[0].resource_id = ssao_resource->handle;
//         bindings[2].resource_id = blur_intermediate_texture;
//         device->update_uniform_set(blur_y_set, bindings, cast_u32(std::size(bindings)));

//         constant_data.ssao_texture_resolution = glm::vec2(cast_float(ssao_resource->resource_info.width), cast_float(ssao_resource->resource_info.height));
//         constant_data.depth_texture_resolution = glm::vec2(cast_float(depth_resource->resource_info.width), cast_float(depth_resource->resource_info.height));
//         constant_data.inv_depth_texture_resolution = 1.0f / constant_data.depth_texture_resolution;
//         constant_data.inv_noise_texture_resolution = 1.0f / glm::vec2(128.0f, 128.0f);
//         constant_data.direction_step = 4.0f;
//         constant_data.neg_inv_r2 = -1.0f;
//         constant_data.radius_to_screen = 2.0f;
//         constant_data.num_step = 8.0f;
//         constant_data.intensity = 2.0f;
//         constant_data.tangent_bias = 0.1f;
//     }

//     void SSAOPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         uint32_t width, height;
//         Window::get()->get_size(&width, &height);
//         float fov = glm::radians(renderer->get_scene()->get_camera()->get_fov());

//         // Convert view space radius to screen space pixels,
//         // y' = y / (z * tan(fov * 0.5))
//         // where y' = ndc height
//         // yscreen = (H * 0.5) * y'
//         // y' = yscreen / (H * 0.5)
//         // yscreen = (y * (H * 0.5)) / (z * tan(fov * 0.5))
//         // We want to calculate the projected radius in pixels, the divide
//         // by z occurs in the shader.
//         float projection_scale = float(height) / (tanf(fov * 0.5f) * 2.0f);
//         constant_data.radius_to_screen = radius * projection_scale;
//         constant_data.neg_inv_r2 = -1.0f * (radius * radius);

//         ScopedCpuProfiling("SSAO Update");
//         ScopedGpuProfiling(command_buffer, "SSAO Pass");
//         device->begin_debug_utils_label(command_buffer, "SSAO Pass", nullptr);

//         Scene *scene = renderer->get_scene();
//         ssao_pass(command_buffer, frame_graph, node, scene);

//         TextureID ssao_texture = frame_graph->get_resource(node->outputs[0])->handle;
//         TextureBarrierInfo barrier_infos[] = {
//             {
//                 .texture_id = ssao_texture,
//                 .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .access_mask = ACCESS_FLAG_SHADER_READ,
//                 .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
//             },
//             {
//                 .texture_id = blur_intermediate_texture,
//                 .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//                 .access_mask = ACCESS_FLAG_SHADER_WRITE,
//                 .layout = IMAGE_LAYOUT_GENERAL,
//             },
//         };
//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         // Blur in x-direction
//         ssao_blur(command_buffer, frame_graph, node, scene, 0);

//         barrier_infos[0].texture_id = blur_intermediate_texture;
//         barrier_infos[1].texture_id = ssao_texture;
//         command_buffer->prepare_image(barrier_infos, cast_u32(std::size(barrier_infos)));

//         // Blur in y-direction
//         ssao_blur(command_buffer, frame_graph, node, scene, 1);
//         device->end_debug_utils_label(command_buffer);
//     }

//     void SSAOPass::ssao_pass(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) {
//         constant_data.inv_projection_matrix = scene->get_camera()->get_inv_projection_transform();
//         PushConstant push_constants = {
//             .data = &constant_data,
//             .offset = 0,
//             .size = sizeof(PushConstants),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         command_buffer->begin_compute_pass(node, frame_graph);
//         ssao_shader->bind(command_buffer);

//         PipelineID pipeline_id = ssao_shader->pipeline_id;
//         command_buffer->set_uniform_sets(pipeline_id, &ssao_set, 1);
//         command_buffer->set_push_constants(pipeline_id, &push_constants, 1);

//         float ssao_push_constant = {};
//         uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
//         uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

//         command_buffer->dispatch(work_size_x, work_size_y, 1);
//     }

//     void SSAOPass::ssao_blur(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene, float direction) {
//         Camera *camera = scene->get_camera();
//         FrameGraphResource *ssao_resource = frame_graph->get_resource(node->outputs[0]);

//         float push_constant_data[] = {
//             cast_float(ssao_resource->resource_info.width),
//             cast_float(ssao_resource->resource_info.height),
//             direction,
//             blur_radius,
//             blur_sharpness,
//             camera->get_near_plane(), camera->get_far_plane()};

//         PushConstant push_constants = {
//             .data = &push_constant_data,
//             .offset = 0,
//             .size = sizeof(float) * std::size(push_constant_data),
//             .shader_stage = SHADER_STAGE_COMPUTE,
//         };

//         UniformSetID uniform_set = direction == 0 ? blur_x_set : blur_y_set;

//         blur_shader->bind(command_buffer);

//         PipelineID pipeline_id = blur_shader->pipeline_id;
//         command_buffer->set_push_constants(pipeline_id, &push_constants, 1);
//         command_buffer->set_uniform_sets(pipeline_id, &uniform_set, 1);
//         float ssao_push_constant = {};
//         uint32_t work_size_x = rendering_utils::get_workgroup_size(node->width, 32);
//         uint32_t work_size_y = rendering_utils::get_workgroup_size(node->height, 32);

//         command_buffer->dispatch(work_size_x, work_size_y, 1);
//     }

//     SSAOPass::~SSAOPass() {
//         if (noise_texture.is_valid())
//             device->destroy_textures(&noise_texture, 1);
//         if (blur_intermediate_texture.is_valid())
//             device->destroy_textures(&blur_intermediate_texture, 1);
//     }

// } // namespace mirai