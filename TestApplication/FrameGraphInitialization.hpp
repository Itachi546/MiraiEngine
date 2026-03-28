#pragma once

#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPass/RenderPass.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Scene/Shader.hpp"
#include "ImGuiService.hpp"

using namespace mirai;

void initialize_forward_pass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
    DepthPrePass depth_prepass{frame_graph, board};

    struct LinearizeDepthPassData {
        FrameGraphResourceHandle depth_texture;
        FrameGraphResourceHandle output;
        Shader *shader;
    };

    // Linearize Depth Pass
    frame_graph->add_callback_pass<LinearizeDepthPassData>(
        "LinearizeDepthPass",
        [=](FrameGraph::FrameGraphBuilder &builder, LinearizeDepthPassData &data) {
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
                                                                           .usage_flags = TEXTURE_USAGE_STORAGE_BIT | TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                                                                       });
            builder.write(data.output,
                          {
                              .access_flags = ACCESS_FLAG_SHADER_READ,
                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                              .layout = IMAGE_LAYOUT_GENERAL,
                          });

            const DepthPrePassData &depth_prepass_data = board->get<DepthPrePassData>();
            builder.read(depth_prepass_data.output,
                         {
                             .access_flags = ACCESS_FLAG_SHADER_READ,
                             .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                         });

            data.depth_texture = depth_prepass_data.output;
            data.shader = Shader::create_from_file("SPIRV/linearize-depth.comp.spv", "LinearizeDepthShader");
            board->add<LinearizeDepthPassData>(data);
        },

        [](const LinearizeDepthPassData &data, FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;

            std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            UniformLayout layouts[] = {
                {
                    .binding = 0,
                    .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
                    .shader_stage = SHADER_STAGE_COMPUTE,
                },
                {
                    .binding = 1,
                    .binding_type = BINDING_TYPE_STORAGE_IMAGE,
                    .shader_stage = SHADER_STAGE_COMPUTE,
                },
            };

            SamplerDescription sampler_desc = SamplerDescription::create();
            sampler_desc.min_filter = sampler_desc.mag_filter = FILTER_NEAREST;
            SamplerID sampler = RenderingDevice::get()->create_sampler(&sampler_desc);

            UniformSetID uniform_set = command_buffer->create_uniform_set(layouts, cast_u32(std::size(layouts)), 0);
            UniformBinding bindings[] = {
                {.resource_id = pass_resource.get<FrameGraphTexture>(data.depth_texture).id, .texture_info = {.sampler = sampler}},
                {.resource_id = pass_resource.get<FrameGraphTexture>(data.output).id},
            };
            RenderingDevice::get()->update_uniform_set(uniform_set, bindings, cast_u32(std::size(bindings)));

            struct PushConstantData {
                uint32_t width;
                uint32_t height;
                float znear;
                float zfar;
            } push_constant_data;

            Camera *camera = ctx->renderer->get_scene()->get_camera();
            push_constant_data.width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            push_constant_data.height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
            push_constant_data.znear = camera->get_near_plane();
            push_constant_data.zfar = camera->get_far_plane();

            PushConstant push_constant = {
                .data = &push_constant_data,
                .offset = 0,
                .size = sizeof(PushConstantData),
                .shader_stage = SHADER_STAGE_COMPUTE,
            };

            command_buffer->bind_pipeline(data.shader->pipeline_id);
            command_buffer->set_uniform_sets(data.shader->pipeline_id, &uniform_set, 1);
            command_buffer->set_push_constants(data.shader->pipeline_id, &push_constant, 1);

            uint32_t work_group_x = rendering_utils::get_workgroup_size(push_constant_data.width, 32);
            uint32_t work_group_y = rendering_utils::get_workgroup_size(push_constant_data.height, 32);
            command_buffer->dispatch(work_group_x, work_group_y, 1);
        });

    // ImGui Pass
    struct ImGuiPassData {
        FrameGraphResourceHandle output;
    };

    frame_graph->add_callback_pass<ImGuiPassData>(
        "ImGuiPass",
        [board](FrameGraph::FrameGraphBuilder &builder, ImGuiPassData &data) {
            const LinearizeDepthPassData &linearize_depth_pass_data = board->get<LinearizeDepthPassData>();
            data.output = linearize_depth_pass_data.output;

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

            uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
            uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

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

/*
  void initialize_frame_graph(FrameGraph *frame_graph) {
      FrameGraphNodeDescription depth_prepass = {
          .name = "depth_prepass",
          .enabled = true,
          .is_compute_pass = false,
          .outputs = {
              FrameGraphResourceOutput{
                  .name = "texture_depth",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .width = 1920,
                  .height = 1080,
                  .array_layers = 1,
                  .format = FORMAT_D32_SFLOAT,
                  .load_op = LOAD_OP_CLEAR,
                  .clear_color = Color{1.0f, 0.0f, 0.0f, 1.0f},
              },

          },
          .renderer = std::make_shared<DepthPrePass>(),
      };
      frame_graph->add_node(depth_prepass);

      FrameGraphNodeDescription forward_pass = {
          .name = "forward_pass",
          .enabled = true,
          .is_compute_pass = false,
          .inputs = {
              FrameGraphResourceInput{
                  .name = "texture_depth",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .load_op = LOAD_OP_LOAD,
              },
          },
          .outputs = {
              FrameGraphResourceOutput{
                  .name = "texture_color",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .width = 1920,
                  .height = 1080,
                  .array_layers = 1,
                  .format = FORMAT_B8G8R8A8_UNORM,
                  .load_op = LOAD_OP_CLEAR,
                  .clear_color = 0x333333ff,
              },
          },
          .renderer = std::make_shared<ForwardPass>(),
      };
      frame_graph->add_node(forward_pass);

      FrameGraphNodeDescription overlay_pass = {
          .name = "overlay3D",
          .enabled = true,
          .is_compute_pass = false,
          .inputs = {
              FrameGraphResourceInput{
                  .name = "texture_depth",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .load_op = LOAD_OP_LOAD,
              },
              FrameGraphResourceInput{
                  .name = "texture_color",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .load_op = LOAD_OP_LOAD,
              },
          },
          .outputs = {
              FrameGraphResourceOutput{
                  .name = "texture_color",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
              },
          },
          .renderer = std::make_shared<Overlay3DPass>(),
      };
      frame_graph->add_node(overlay_pass);

      FrameGraphNodeDescription swapchain_copy_pass = {
          .name = "swapchain_copy",
          .enabled = true,
          .is_compute_pass = false,
          .inputs = {
              FrameGraphResourceInput{
                  .name = "texture_color",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_TEXTURE,
              },
          },
          .outputs = {
              FrameGraphResourceOutput{
                  .name = "swapchain",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
                  .load_op = LOAD_OP_CLEAR,
              },
          },
          .renderer = std::make_shared<SwapchainCopyPass>(),
      };
      frame_graph->add_node(swapchain_copy_pass);

      FrameGraphNodeDescription imgui_pass = {
          .name = "imgui_pass",
          .enabled = true,
          .is_compute_pass = false,
          .inputs = {
              FrameGraphResourceInput{
                  .name = "swapchain",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_ATTACHMENT,
              },
          },
          .outputs = {
              FrameGraphResourceOutput{
                  .name = "swapchain",
                  .resource_type = FRAMEGRAPH_RESOURCE_TYPE_REFERENCE,
                  .load_op = LOAD_OP_CLEAR,
              },
          },
          .renderer = std::make_shared<ImGuiRenderPass>(),
      };
      frame_graph->add_node(imgui_pass);
  }

  */
