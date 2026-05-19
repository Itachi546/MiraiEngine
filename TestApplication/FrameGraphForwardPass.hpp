#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "ImGuiService.hpp"
#include "Graphics/GPUResource.hpp"
#include "Scene/Material.hpp"

using namespace mirai;

void initialize_forward_pass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
    DepthPrePass depth_prepass{frame_graph, board};
    ViewNormalDepthPass view_pass{frame_graph, board};
    RTShadowPass shadow_pass{frame_graph, board};
    SSAOPass ssao_pass{frame_graph, board};
    CascadedShadowPass cascaded_shadow_pass{frame_graph, board};
    TiledLightCullingPass light_cull_pass{frame_graph, board};
    ForwardPass forward_pass{frame_graph, board};
    BloomPass bloom_pass{frame_graph, board};
    TAAResolvePass taa_pass{frame_graph, board};
    FinalCompositePass composite_pass{frame_graph, board};
    // DeferredOverlay3DPass overlay3d_pass{frame_graph, board};

    /*
    struct LinearizeDepthPassData {
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    // Linearize Depth Pass
    frame_graph->add_callback_pass<LinearizeDepthPassData>(
        "LinearizeDepthPass",
        [=](FrameGraph::Builder &builder, LinearizeDepthPassData &data) {
            uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
            data.output = builder.create_texture("LinearDepthTexture", {
                                                                           .create_flags = 0,
                                                                           .width = width,
                                                                           .height = height,
                                                                           .depth = 1,
                                                                           .mip_levels = 1,
                                                                           .array_layers = 1,
                                                                           .texture_type = TEXTURE_TYPE_2D,
                                                                           .format = FORMAT_B8G8R8A8_UNORM,
                                                                           .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                                                                       });
            builder.write(data.output,
                          {
                              .access_flags = ACCESS_FLAG_SHADER_READ,
                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              .layout = IMAGE_LAYOUT_GENERAL,
                          });

            const DepthPrePassData &depth_prepass = board->get<DepthPrePassData>();
            builder.read(depth_prepass.output, {
                                                   .access_flags = ACCESS_FLAG_SHADER_READ,
                                                   .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                   .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                               });
            data.depth_texture = depth_prepass.output;

            data.shader = std::make_shared<ComputeShader>("LinearizeDepthShader", "SPIRV/linearize-depth.comp.spv");
            board->add<LinearizeDepthPassData>(data);

            builder.present(data.output);
        },

        [](const LinearizeDepthPassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;
            Renderer *renderer = ctx->renderer;

            std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            struct PushConstantData {
                uint32_t width;
                uint32_t height;
                float znear;
                float zfar;
                uint32_t depth_texture_index;
                uint32_t padding[3];
            } push_constant_data;

            Camera *camera = ctx->renderer->get_scene()->get_camera();
            push_constant_data.width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            push_constant_data.height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
            push_constant_data.znear = camera->get_near_plane();
            push_constant_data.zfar = camera->get_far_plane();
            push_constant_data.depth_texture_index = pass_resource.get<FrameGraphTexture>(data.depth_texture).id.id;

            DescriptorOffset bindings[] = {
                renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
            };

            data.shader->bind(command_buffer);
            command_buffer->set_push_data(0, &push_constant_data, sizeof(PushConstantData));
            command_buffer->set_push_data(sizeof(PushConstantData), bindings, cast_u32(sizeof(bindings)));

            uint32_t work_group_x = rendering_utils::get_workgroup_size(push_constant_data.width, 32);
            uint32_t work_group_y = rendering_utils::get_workgroup_size(push_constant_data.height, 32);
            command_buffer->dispatch(work_group_x, work_group_y, 1);
        });
    /*
    struct CopyTexturePassData {
        FrameGraphResourceHandle ssao_texture;
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    struct CopyTexturePassBindings {
        uint32_t descriptors[2];
    };
    // Copy Texture Pass
    frame_graph->add_callback_pass<CopyTexturePassData>(
        "CopyTexturePass",
        [=](FrameGraph::FrameGraphBuilder &builder, CopyTexturePassData &data) {
            uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

            data.output = builder.create_texture("OutputTexture", {
                                                                      .create_flags = 0,
                                                                      .width = width,
                                                                      .height = height,
                                                                      .depth = 1,
                                                                      .mip_levels = 1,
                                                                      .array_layers = 1,
                                                                      .texture_type = TEXTURE_TYPE_2D,
                                                                      .format = FORMAT_B8G8R8A8_UNORM,
                                                                      .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                                                                  });
            builder.write(data.output,
                          {
                              .access_flags = ACCESS_FLAG_SHADER_READ,
                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              .layout = IMAGE_LAYOUT_GENERAL,
                          });

            data.shader = std::make_shared<ComputeShader>("CopyTextureShader", "SPIRV/copy-texture.comp.spv");
            board->add<CopyTexturePassData>(data);
        },

        [](const CopyTexturePassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;
            Renderer *renderer = ctx->renderer;
            EnvironmentMap *env_map = renderer->get_scene()->get_environment_map();

            std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            FrameGraphBlackBoard *board = Renderer::get()->get_frame_graph_blackboard();
            CopyTexturePassBindings *bindings = nullptr;
            if (!board->has<CopyTexturePassBindings>()) {
                DescriptorInfo descriptor_infos[] = {
                    {.type = DescriptorType::SampledImage, .resource = env_map->get_brdf_texture(), .image_info = {0, ~0u, 0, ~0u}},
                    {.type = DescriptorType::StorageImage, .resource = pass_resource.get<FrameGraphTexture>(data.output).id, .image_info = {0, ~0u, 0, ~0u}},
                };

                DescriptorOffset base_descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), descriptor_infos, cast_u32(std::size(descriptor_infos)));
                bindings = &board->add<CopyTexturePassBindings>(CopyTexturePassBindings{
                    .descriptors = {base_descriptor_offset, base_descriptor_offset + 1},
                });
            } else {
                bindings = &board->get<CopyTexturePassBindings>();
            }

            ASSERT(bindings != nullptr);
            uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

            uint32_t push_constants[] = {width, height, 0, 0};

            data.shader->bind(command_buffer);
            command_buffer->set_push_data(0, push_constants, cast_u32(sizeof(push_constants)));
            command_buffer->set_push_data(sizeof(push_constants), bindings->descriptors, cast_u32(sizeof(bindings->descriptors)));
            uint32_t work_group_x = rendering_utils::get_workgroup_size(width + 1, 32);
            uint32_t work_group_y = rendering_utils::get_workgroup_size(height + 1, 32);
            command_buffer->dispatch(work_group_x, work_group_y, 1);
        });
    */
    /*
    struct LinearizeDepthPassData {
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle output;
        std::shared_ptr<ComputeShader> shader;
    };

    struct LinearizeDepthPassBindings {
        uint32_t descriptors[2];
    };

    // Linearize Depth Pass
    frame_graph->add_callback_pass<LinearizeDepthPassData>(
        "LinearizeDepthPass",
        [=](FrameGraph::Builder &builder, LinearizeDepthPassData &data) {
            uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
            data.output = builder.create_texture("LinearDepthTexture", {
                                                                           .create_flags = 0,
                                                                           .width = width,
                                                                           .height = height,
                                                                           .depth = 1,
                                                                           .mip_levels = 1,
                                                                           .array_layers = 1,
                                                                           .texture_type = TEXTURE_TYPE_2D,
                                                                           .format = FORMAT_B8G8R8A8_UNORM,
                                                                           .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | TEXTURE_USAGE_SAMPLED_BIT,
                                                                       });
            builder.write(data.output,
                          {
                              .access_flags = ACCESS_FLAG_SHADER_READ,
                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              .layout = IMAGE_LAYOUT_GENERAL,
                          });

            data.shader = std::make_shared<ComputeShader>("LinearizeDepthShader", "SPIRV/rt-linearize-depth.comp.spv");
            board->add<LinearizeDepthPassData>(data);

            builder.present(data.output);
        },

        [](const LinearizeDepthPassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;
            Renderer *renderer = ctx->renderer;

            std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            struct PushConstantData {
                glm::mat4 invP;
                glm::mat4 invV;
                glm::vec4 camera_position;
                uint32_t width;
                uint32_t height;
                float znear;
                float zfar;
            } push_constant_data;

            Camera *camera = ctx->renderer->get_scene()->get_camera();
            push_constant_data.invP = camera->get_inv_projection_transform();
            push_constant_data.invV = camera->get_inv_view_transform();
            push_constant_data.camera_position = glm::vec4(camera->position, 1.0f);
            push_constant_data.width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            push_constant_data.height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
            push_constant_data.znear = camera->get_near_plane();
            push_constant_data.zfar = camera->get_far_plane();

            DescriptorOffset bindings[] = {
                renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.output).id, DescriptorType::StorageImage),
                renderer->get_or_create_descriptor(renderer->tlas.as, DescriptorType::AccelerationStructure),
            };

            data.shader->bind(command_buffer);
            command_buffer->set_push_data(0, &push_constant_data, sizeof(PushConstantData));
            command_buffer->set_push_data(sizeof(PushConstantData), bindings, cast_u32(sizeof(bindings)));

            uint32_t work_group_x = rendering_utils::get_workgroup_size(push_constant_data.width, 32);
            uint32_t work_group_y = rendering_utils::get_workgroup_size(push_constant_data.height, 32);
            command_buffer->dispatch(work_group_x, work_group_y, 1);
        });
        */
    // ImGui Pass
    struct ImGuiPassData {
        FrameGraphResourceHandle output;
    };

    frame_graph->add_callback_pass<ImGuiPassData>(
        "ImGuiPass",
        [board](FrameGraph::Builder &builder, ImGuiPassData &data) {
            const FinalCompositePassData &input_pass = board->get<FinalCompositePassData>();
            data.output = input_pass.output;

            builder.write(data.output, {
                                           .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE | ACCESS_FLAG_COLOR_ATTACHMENT_READ,
                                           .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });
            builder.present(data.output);
        },
        [](const ImGuiPassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;

            ScopedGpuProfiling(command_buffer, "ImGui Pass");
            command_buffer->begin_gpu_debug_label("ImGuiPass");

            const auto &resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            uint32_t width, height;
            Window::get()->get_size(&width, &height);

            const FrameGraphTexture &texture = pass_resource.get<FrameGraphTexture>(data.output);
            command_buffer->begin_render_pass({
                                                  AttachmentInfo{
                                                      .texture = texture.id,
                                                      .load_op = LOAD_OP_LOAD,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                                                  },
                                              },
                                              std::nullopt, width, height);

            command_buffer->set_viewport({
                .x = 0.0f,
                .y = 0.0f,
                .width = cast_float(width),
                .height = cast_float(height),
                .min_depth = 0.0,
                .max_depth = 1.0,
            });

            command_buffer->set_scissor(0, 0, width, height);

            ImGuiService::Render(command_buffer);

            command_buffer->end_render_pass();
            command_buffer->end_gpu_debug_label();
        });
}