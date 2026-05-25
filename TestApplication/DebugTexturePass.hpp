#pragma once
#include "RenderPass/RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"

struct DebugPassData {
    std::shared_ptr<EffectMaterial> shader;
    FrameGraphResourceHandle output;
    FrameGraphResourceHandle input;
};

void add_texture_debug_pass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {

    frame_graph->add_callback_pass<DebugPassData>(
        "DebugPass",
        [board](FrameGraph::Builder &builder, DebugPassData &data) {
            // @TODO we can skip the creation of this texture by copying directly to swapchain
            uint32_t width = AppSettings::get_width();
            uint32_t height = AppSettings::get_height();

            data.output = builder.add_texture(K_SWAPCHAIN_TEXTURE_HANDLE, "Swapchain");

            builder.write(data.output, {
                                           .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE,
                                           .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                           .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                       });

            const ForwardPassData &input_pass_data = board->get<ForwardPassData>();
            builder.read(input_pass_data.color_texture, {
                                                     .access_flags = ACCESS_FLAG_SHADER_READ,
                                                     .stage_mask = PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                                     .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                 });
            data.input = input_pass_data.color_texture;

            data.shader = std::make_shared<EffectMaterial>("FinalCompositeShader",
                                                           std::vector<std::string>{"SPIRV/fullscreen.vert.spv", "SPIRV/debug.frag.spv"},
                                                           PipelineState{
                                                               .cull_mode = CULL_MODE_NONE,
                                                               .depth_test = false,
                                                           },
                                                           PipelineAttachmentInfo{
                                                               .color_attachments_format = {FORMAT_B8G8R8A8_UNORM},
                                                           });
            ASSERT(data.shader != nullptr);
            board->add<DebugPassData>(data);
        },

        [](const DebugPassData &data, const FrameGraphPassResource &pass_resource, void *context) {
            RenderContext *ctx = static_cast<RenderContext *>(context);
            CommandBuffer *command_buffer = ctx->command_buffer;

            auto resource_states = pass_resource.get_resource_access_states();
            command_buffer->prepare_resources(resource_states);

            uint32_t width = AppSettings::get_width();
            uint32_t height = AppSettings::get_height();

            Renderer *renderer = Renderer::get();
            FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();

            const FrameGraphTexture &texture = pass_resource.get<FrameGraphTexture>(data.input);
            TextureID input_color_texture = texture.id;
            Format format = texture.desc.format;

            uint32_t num_channel = 4;
            if (format == FORMAT_R16_SFLOAT || format == FORMAT_D32_SFLOAT)
                num_channel = 1;
            else if (format == FORMAT_R16G16_SFLOAT)
                num_channel = 0;

            ASSERT(input_color_texture.is_valid());

            struct PushData {
                uint32_t texture_id;
                uint32_t num_channel;
                uint32_t _padding[2];
            } push_data;
            push_data.texture_id = input_color_texture.id;
            push_data.num_channel = num_channel;

            command_buffer->begin_gpu_debug_label("DebugPass");
            ScopedGpuProfiling(command_buffer, "DebugPass");

            uint32_t swapchain_width, swapchain_height;
            Window::get()->get_size(&swapchain_width, &swapchain_height);
            command_buffer->begin_render_pass({AttachmentInfo{
                                                  .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                  .load_op = LOAD_OP_CLEAR,
                                                  .store_op = STORE_OP_STORE,
                                                  .clear_color = {0.0f, 0.0f, 0.0f, 0.0f},
                                              }},
                                              {}, swapchain_width, swapchain_height);
            command_buffer->set_viewport({
                .x = 0.0f,
                .y = 0.0f,
                .width = cast_float(swapchain_width),
                .height = cast_float(swapchain_height),
                .min_depth = 0.0f,
                .max_depth = 1.0f,
            });
            command_buffer->set_scissor(0, 0, swapchain_width, swapchain_height);

            data.shader->bind(command_buffer);
            command_buffer->set_push_data(0, &push_data, cast_u32(sizeof(push_data)));

            command_buffer->draw(3, 1, 0, 0);

            command_buffer->end_render_pass();

            command_buffer->end_gpu_debug_label();
        });
}