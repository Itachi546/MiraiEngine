#include "DDGIPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Renderer.hpp"

namespace mirai {

    struct DDGIGenerateIrradiancePassData {
        FrameGraphResourceHandle irradiance_texture;
        FrameGraphResourceHandle depth_texture;
    };

    DDGIPass::DDGIPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<DDGIGenerateIrradiancePassData>(
            "DDGI Generate Irradiance",
            [&board](FrameGraph::Builder &builder, DDGIGenerateIrradiancePassData &data) {
                IrradianceFieldSettings irradiance_settings;
                board->add<IrradianceFieldSettings>(irradiance_settings);

                // For each probe, we add 1 pixel border, also we add one pixel border around the whole texture
                glm::uvec3 probe_counts = irradiance_settings.probe_counts;
                uint32_t irradiance_oct_res = irradiance_settings.irradiance_oct_resolution;
                uint32_t irradiance_width = (irradiance_oct_res + 2) * probe_counts.x * probe_counts.y + 2;
                uint32_t irradiance_height = (irradiance_oct_res + 2) * probe_counts.z + 2;
                data.irradiance_texture = builder.create_texture("DDGIIrradianceTexture", {
                                                                                              .create_flags = 0,
                                                                                              .width = irradiance_width,
                                                                                              .height = irradiance_height,
                                                                                              .depth = 1,
                                                                                              .mip_levels = 1,
                                                                                              .array_layers = 1,
                                                                                              .texture_type = TEXTURE_TYPE_2D,
                                                                                              .format = FORMAT_R11G11B10_FLOAT,
                                                                                              .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                          });
                builder.write(data.irradiance_texture, {
                                                           .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                           .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                           .layout = IMAGE_LAYOUT_GENERAL,
                                                       });

                uint32_t depth_oct_res = irradiance_settings.depth_oct_resolution;
                uint32_t depth_width = (depth_oct_res + 2) * probe_counts.x * probe_counts.y + 2;
                uint32_t depth_height = (depth_oct_res + 2) * probe_counts.z + 2;
                data.depth_texture = builder.create_texture("DDGIDepthTexture", {
                                                                                    .create_flags = 0,
                                                                                    .width = depth_width,
                                                                                    .height = depth_height,
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

                board->add<DDGIGenerateIrradiancePassData>(data);

                if (irradiance_settings.enable_debug_probe) {
                    Renderer *renderer = Renderer::get();
                    Scene *scene = renderer->get_scene();
                    auto &component_manager = scene->ecs->component_manager;
                    IrradianceFieldSettings settings;
                    Entity parent = scene->create_entity("Probes");
                    const float radius = 0.1f;

                    uint32_t material_index = cast_u32(scene->materials.size());
                    {
                        std::unique_ptr<Material3D> probe_material = std::make_unique<Material3D>("ProbeMaterial");
                        probe_material->properties.albedo = glm::vec4(1.0f, 0.0f, 0.49f, 1.0f);
                        scene->materials.push_back(std::move(probe_material));
                    }

                    uint32_t probe_count = settings.get_probe_count();
                    for (uint32_t i = 0; i < probe_count; ++i) {
                        Entity entity = scene->create_sphere("Probe" + std::to_string(i), parent);
                        TransformComponent *component = component_manager->get_component<TransformComponent>(entity);
                        component->position = settings.probe_index_to_position(i);
                        component->scale = glm::vec3(radius);

                        MeshComponent *mesh_comp = component_manager->get_component<MeshComponent>(entity);
                        mesh_comp->flags = MeshComponent::Flags::MESH_FLAG_NONE;
                        mesh_comp->primitives[0].material = material_index;
                    }
                }
            },
            [](const DDGIGenerateIrradiancePassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                return;
            });
    }
} // namespace mirai