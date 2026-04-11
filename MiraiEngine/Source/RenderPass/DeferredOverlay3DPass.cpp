#include "DeferredOverlay3DPass.hpp"
#include "RenderPassData.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/Camera.hpp"
#include "Graphics/Renderer.hpp"
#include "Scene/EnvironmentMap.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"

namespace mirai {

    struct DeferredOverlay3DPassBindings {
        DescriptorOffset descriptor;
    };

    DeferredOverlay3DPass::DeferredOverlay3DPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<DeferredOverlay3DPassData>(
            "Overlay3DPass",
            [=](FrameGraph::FrameGraphBuilder &builder, DeferredOverlay3DPassData &data) {
                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);
                data.output = builder.create_texture("Overlay3DPass", {
                                                                          .create_flags = 0,
                                                                          .width = width,
                                                                          .height = height,
                                                                          .depth = 1,
                                                                          .mip_levels = 1,
                                                                          .array_layers = 1,
                                                                          .texture_type = TEXTURE_TYPE_2D,
                                                                          .format = FORMAT_B8G8R8A8_UNORM,
                                                                          .usage_flags = TEXTURE_USAGE_TRANSFER_SRC_BIT | TEXTURE_USAGE_COLOR_ATTACHMENT_BIT,
                                                                      });
                builder.write(data.output,
                              {
                                  .access_flags = ACCESS_FLAG_COLOR_ATTACHMENT_WRITE | ACCESS_FLAG_COLOR_ATTACHMENT_READ,
                                  .stage_mask = PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  .layout = IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                              });

                data.skybox_shader = std::make_shared<ShaderMaterial>("OverlaySkyboxShader",
                                                                      std::vector<std::string>{"SPIRV/fullscreen.vert.spv", "SPIRV/skybox.frag.spv"},
                                                                      PipelineState{
                                                                          .cull_mode = CULL_MODE_NONE,
                                                                          .depth_test = true,
                                                                      },
                                                                      PipelineAttachmentInfo{
                                                                          .color_attachments_format = {FORMAT_B8G8R8A8_UNORM},
                                                                          .has_depth_attachment = false,
                                                                      });
                board->add<DeferredOverlay3DPassData>(data);
            },

            [](const DeferredOverlay3DPassData &data, FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                CommandBuffer *command_buffer = ctx->command_buffer;
                Renderer *renderer = ctx->renderer;

                Camera *camera = renderer->get_scene()->get_camera();
                EnvironmentMap *env_map = renderer->get_scene()->get_environment_map();

                FrameGraphBlackBoard *board = Renderer::get()->get_frame_graph_blackboard();
                DeferredOverlay3DPassBindings *bindings = nullptr;
                if (!board->has<DeferredOverlay3DPassBindings>()) {
                    DescriptorInfo descriptor_info = {.type = DescriptorType::SampledImage, .resource = env_map->get_cubemap(), .image_info = {0, 1, 0, 6}};
                    DescriptorOffset descriptor_offset = renderer->resource_heap.push_descriptors(RenderingDevice::get(), &descriptor_info, 1);
                    bindings = &board->add<DeferredOverlay3DPassBindings>(DeferredOverlay3DPassBindings{
                        .descriptor = descriptor_offset,
                    });
                } else {
                    bindings = &board->get<DeferredOverlay3DPassBindings>();
                }
                ASSERT(bindings != nullptr);

                struct PushConstantData {
                    glm::mat4 invP;
                    glm::mat4 invV;
                } push_constant_data;

                push_constant_data.invP = camera->get_inv_projection_transform();
                push_constant_data.invV = camera->get_inv_view_transform();

                uint32_t width = cast_u32(AppSettings::default_window_width * AppSettings::resolution_scale);
                uint32_t height = cast_u32(AppSettings::default_window_height * AppSettings::resolution_scale);

                std::vector<ResourceAccessDeclaration> resource_states = pass_resource.get_resource_access_states();
                command_buffer->prepare_resources(resource_states);
                command_buffer->begin_gpu_debug_label("DeferredOverlay3DPass");
                ScopedGpuProfiling(command_buffer, "DeferredOverlay3DPass");

                command_buffer->begin_render_pass({{
                                                      .texture = pass_resource.get<FrameGraphTexture>(data.output).id,
                                                      // @TODO should be load op load
                                                      .load_op = LOAD_OP_CLEAR,
                                                      .store_op = STORE_OP_STORE,
                                                      .clear_color = {0.0f, 0.0f, 0.0f, 1.0f},
                                                  }},
                                                  std::nullopt, width, height);
                command_buffer->set_viewport({0.0f, 0.0f, cast_float(width), cast_float(height), 0.0f, 1.0f});
                command_buffer->set_scissor(0, 0, width, height);

                data.skybox_shader->bind(command_buffer);
                command_buffer->set_push_data(0, &push_constant_data, sizeof(PushConstantData));
                command_buffer->set_push_data(sizeof(PushConstantData), &bindings->descriptor, cast_u32(sizeof(uint32_t)));

                command_buffer->draw(3, 1, 0, 0);

                command_buffer->end_render_pass();
                command_buffer->end_gpu_debug_label();
            });
    }
}; // namespace mirai

// #include "Overlay3DPass.hpp"
// #include "Graphics/Vulkan/CommandBuffer.hpp"
// #include "Engine/Profiler.hpp"
// #include "Scene/Camera.hpp"
// #include "Scene/ShaderHashMap.hpp"
// #include "Scene/EnvironmentMap.hpp"
// #include "Graphics/Renderer.hpp"

// namespace mirai {
//     Overlay3DPass::Overlay3DPass() : FrameGraphRenderer("sky_pass"), skybox_shader(nullptr) {
//     }

//     void Overlay3DPass::initialize(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         PipelineState pipeline_state = {};
//         pipeline_state.render_state.fields.pass_mode = SHADER_PASS_SKYBOX;
//         pipeline_state.render_state.fields.depth_test = true;
//         skybox_shader = ShaderHashMap::get()->get(pipeline_state.get_hash());

//         SamplerDescription sampler_desc = SamplerDescription::create();
//         default_sampler = device->create_sampler(&sampler_desc);

//         UniformLayout layout = {
//             .binding = 0,
//             .binding_type = BINDING_TYPE_COMBINED_IMAGE_SAMPLER,
//             .shader_stage = SHADER_STAGE_FRAGMENT,
//         };
//         skybox_uniform_set = device->create_uniform_set(&layout, 1, 0, "skybox_binding");
//         TextureID skybox = renderer->get_scene()->get_environment_map()->get_cubemap();
//         UniformBinding binding = {.resource_id = skybox, .texture_info = {.sampler = default_sampler}};
//         device->update_uniform_set(skybox_uniform_set, &binding, 1);

//         line_renderer = std::make_unique<LineRenderer>();
//     }

//     void Overlay3DPass::update(FrameGraph *frame_graph, const FrameGraphNode *node, Renderer *renderer) {
//         line_renderer->NewFrame();
//     }

//     void Overlay3DPass::render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer *renderer) {
//         device->begin_debug_utils_label(command_buffer, "Overlay3D", nullptr);
//         ScopedGpuProfiling(command_buffer, "Overlay3D");

//         command_buffer->begin_render_pass(node, frame_graph);

//         Scene *scene = renderer->get_scene();
//         if (scene->get_environment_map() != nullptr)
//             render_skybox(command_buffer, scene);

//         render_debug_draw(command_buffer, scene);

//         command_buffer->end_render_pass();

//         device->end_debug_utils_label(command_buffer);
//     }

//     void Overlay3DPass::render_skybox(CommandBuffer *command_buffer, Scene *scene) {
//         Camera *camera = scene->get_camera();

//         // Draw Sky
//         skybox_shader->bind(command_buffer);

//         command_buffer->set_uniform_sets(skybox_shader->pipeline_id, &skybox_uniform_set, 1);

//         glm::mat4 push_constant_data[] = {camera->get_inv_projection_transform(), camera->get_inv_view_transform()};
//         PushConstant push_constant = {
//             .data = push_constant_data,
//             .offset = 0,
//             .size = sizeof(glm::mat4) * 2,
//             .shader_stage = SHADER_STAGE_FRAGMENT,
//         };

//         command_buffer->set_push_constants(skybox_shader->pipeline_id, &push_constant, 1);
//         command_buffer->draw(3, 1, 0, 0);
//     }

//     void Overlay3DPass::render_debug_draw(CommandBuffer *command_buffer, Scene *scene) {
//         // Draw Lines
//         if (line_renderer->line_count > 0) {
//             Camera *camera = scene->get_camera();
//             glm::mat4 VP = camera->get_view_projection_transform();
//             PushConstant push_constant = {
//                 .data = &VP[0][0],
//                 .offset = 0,
//                 .size = sizeof(glm::mat4),
//                 .shader_stage = SHADER_STAGE_VERTEX,
//             };

//             line_renderer->shader->bind(command_buffer);
//             command_buffer->set_push_constants(line_renderer->shader->pipeline_id, &push_constant, 1);

//             command_buffer->draw(line_renderer->line_count * 2, 1, 0, 0);
//         }
//     }
// } // namespace mirai