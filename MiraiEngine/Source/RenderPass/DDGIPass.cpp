#include "DDGIPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/Material.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Common/Profiler.hpp"
#include "Common/Random.hpp"
#include "Math/Math.hpp"
namespace mirai {

    DDGIPass::DDGIPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<DDGIGenerateIrradiancePassData>(
            "DDGI Generate Irradiance",
            [&board](FrameGraph::Builder &builder, DDGIGenerateIrradiancePassData &data) {
                IrradianceFieldSettings irradiance_settings{glm::uvec3(24, 11, 15), AABB{
                                                                                        glm::vec3(-15.75f, -1.5f, -10.0f),
                                                                                        glm::vec3(14.75f, 11.5f, 9.0f),
                                                                                    }};
                board->add<IrradianceFieldSettings>(irradiance_settings);

                // For each probe, we add 1 pixel border, also we add one pixel border around the whole texture
                glm::uvec3 probe_counts = irradiance_settings.probe_counts;
                data.radiance_texture = builder.create_texture("DDGIRadianceTexture", {
                                                                                          .create_flags = 0,
                                                                                          .width = irradiance_settings.rays_per_probe,
                                                                                          .height = irradiance_settings.get_probe_count(),
                                                                                          .depth = 1,
                                                                                          .mip_levels = 1,
                                                                                          .array_layers = 1,
                                                                                          .texture_type = TEXTURE_TYPE_2D,
                                                                                          .format = FORMAT_R16G16B16A16_SFLOAT,
                                                                                          .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                      });
                builder.write(data.radiance_texture, {
                                                         .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                         .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                         .layout = IMAGE_LAYOUT_GENERAL,
                                                     });

                data.depth_texture = builder.create_texture("DDGIDepthTexture", {
                                                                                    .create_flags = 0,
                                                                                    .width = irradiance_settings.rays_per_probe,
                                                                                    .height = irradiance_settings.get_probe_count(),
                                                                                    .depth = 1,
                                                                                    .mip_levels = 1,
                                                                                    .array_layers = 1,
                                                                                    .texture_type = TEXTURE_TYPE_2D,
                                                                                    .format = FORMAT_R16G16_SFLOAT,
                                                                                    .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                });

                builder.write(data.depth_texture, {
                                                      .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                      .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                      .layout = IMAGE_LAYOUT_GENERAL,
                                                  });

                data.shader = std::make_shared<RTShader>("DDGIRadianceShader",
                                                         "SPIRV/ddgi.rgen.spv",
                                                         std::vector<std::string>{"SPIRV/ddgi.rchit.spv"},
                                                         std::vector<std::string>{"SPIRV/ddgi.rmiss.spv"},
                                                         1);

                board->add<DDGIGenerateIrradiancePassData>(data);

                // irradiance_settings.enable_debug_probe = false;
                if (irradiance_settings.enable_debug_probe) {
                    Renderer *renderer = Renderer::get();
                    Scene *scene = renderer->get_scene();
                    auto &component_manager = scene->ecs->component_manager;
                    Entity parent = scene->create_entity("Probes");
                    const float radius = 0.2f;

                    uint32_t material_index = cast_u32(scene->materials.size());
                    {
                        std::unique_ptr<Material3D> probe_material = std::make_unique<Material3D>("ProbeMaterial");
                        probe_material->properties.albedo = glm::vec4(1.0f, 0.0f, 0.49f, 1.0f);
                        scene->materials.push_back(std::move(probe_material));
                    }

                    uint32_t probe_count = irradiance_settings.get_probe_count();
                    for (uint32_t i = 0; i < probe_count; ++i) {
                        Entity entity = scene->create_sphere("Probe" + std::to_string(i), parent);
                        TransformComponent *component = component_manager->get_component<TransformComponent>(entity);
                        component->position = irradiance_settings.probe_index_to_position(i);
                        component->scale = glm::vec3(radius);

                        MeshComponent *mesh_comp = component_manager->get_component<MeshComponent>(entity);
                        mesh_comp->flags = MeshComponent::Flags::MESH_FLAG_DEBUG;
                        mesh_comp->primitives[0].material = material_index;
                    }
                }
            },
            [](const DDGIGenerateIrradiancePassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                if (!renderer->tlas.as.is_valid())
                    return;

                ScopedGpuProfiling(command_buffer, "DDGIRadiancePass");
                command_buffer->begin_gpu_debug_label("DDGIRadiancePass");

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const IrradianceFieldSettings &settings = board->get<IrradianceFieldSettings>();

                struct PushData {
                    glm::mat4 random_orientation;

                    glm::ivec3 probe_counts;
                    uint32_t ray_per_probe;

                    glm::vec3 probe_start_position;
                    uint32_t frame_count;

                    glm::vec3 probe_step;
                    float _padding1;
                } push_data;

                push_data.random_orientation = glm::mat4_cast(glm::angleAxis(
                    random_float01() * (glm::pi<float>() * 2.0f),
                    glm::normalize(glm::vec3(random_float01() * 2.0f - 1.0f,
                                             random_float01() * 2.0f - 1.0f,
                                             random_float01() * 2.0f - 1.0f))));
                push_data.probe_counts = settings.probe_counts;
                push_data.ray_per_probe = settings.rays_per_probe;
                push_data.probe_start_position = settings.probe_start_position;
                push_data.probe_step = settings.probe_step;
                push_data.frame_count = cast_u32(renderer->frame_id & UINT32_MAX);

                BufferID buffer = renderer->geometry_buffer_allocator->allocations[0].id;
                DescriptorOffset geometry_descriptor = renderer->get_or_create_descriptor(buffer, DescriptorType::StorageBuffer);

                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(renderer->tlas.as, DescriptorType::AccelerationStructure),
                    renderer->per_frame_data_descriptor,
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.radiance_texture).id, DescriptorType::StorageImage),
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.depth_texture).id, DescriptorType::StorageImage),
                    geometry_descriptor,
                    renderer->rt_instance_data_descriptor,
                    renderer->material_descriptor,
                    renderer->light_descriptor,

                };

                data.shader->bind(command_buffer);

                uint32_t push_data_size = cast_u32(sizeof(push_data));
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                uint32_t width = settings.rays_per_probe;
                uint32_t height = settings.get_probe_count();

                command_buffer->trace_rays(width, height, 1);

                command_buffer->end_gpu_debug_label();

                return;
            });
    }
} // namespace mirai