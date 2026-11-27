#include "DDGIPass.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Scene/FrameGraphBlackBoard.hpp"
#include "Scene/ShaderRegistry.hpp"
#include "RenderPassData.hpp"
#include "Graphics/Renderer.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Common/Profiler.hpp"
#include "Common/Random.hpp"
#include "Math/Math.hpp"
#include "Graphics/RenderingDevice.hpp"
namespace mirai {

    constexpr bool K_DDGI_RANDOM_RAY_ROTATION = false;

    static glm::mat4 GetDDGIRayRotation(uint64_t frame_id) {
        if (!K_DDGI_RANDOM_RAY_ROTATION)
            return glm::mat4(1.0f);

        std::mt19937 rng(static_cast<uint32_t>(frame_id & UINT32_MAX));
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        float angle = dist(rng) * glm::pi<float>() * 2.0f;
        glm::vec3 axis = glm::vec3(dist(rng), dist(rng), dist(rng)) * 2.0f - 1.0f;
        if (glm::length(axis) < 1e-4f)
            axis = glm::vec3(0.0f, 1.0f, 0.0f);
        return glm::mat4_cast(glm::angleAxis(angle, glm::normalize(axis)));
    }

    DDGIPass::DDGIPass(FrameGraph *frame_graph, FrameGraphBlackBoard *board) {
        frame_graph->add_callback_pass<DDGIGenerateRadiancePassData>(
            "DDGI Generate Radiance",
            [&board](FrameGraph::Builder &builder, DDGIGenerateRadiancePassData &data) {
                IrradianceFieldSettings irradiance_settings{glm::uvec3(32, 8, 16), AABB{
                                                                                       glm::vec3(-15.75f, -1.5f, -10.0f),
                                                                                       glm::vec3(14.75f, 11.5f, 9.0f),
                                                                                   }};

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
                                                         .stage_mask = PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
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
                                                      .stage_mask = PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                                                      .layout = IMAGE_LAYOUT_GENERAL,
                                                  });

                data.shader = std::make_shared<RTShader>("DDGIRadianceShader",
                                                         "SPIRV/ddgi.rgen.spv",
                                                         std::vector<std::string>{"SPIRV/ddgi.rchit.spv"},
                                                         std::vector<std::string>{"SPIRV/ddgi.rmiss.spv"},
                                                         1);

                board->add<DDGIGenerateRadiancePassData>(data);

                // irradiance_settings.enable_debug_probe = false;
                Renderer *renderer = Renderer::get();
                Scene *scene = renderer->get_scene();
                auto &component_manager = scene->ecs->component_manager;
                Entity parent = scene->create_entity("Probes");
                const glm::vec3 radius = glm::vec3(0.2f);

                uint32_t material_index = cast_u32(scene->materials.size());
                {

                    PipelineAttachmentInfo attachment_info = get_forward_pass_attachment_info();
                    std::unique_ptr<ShaderMaterial3D> probe_material = std::make_unique<ShaderMaterial3D>("ProbeMaterial",
                                                                                                          std::vector<std::string>{"SPIRV/visualize-probe.vert.spv", "SPIRV/visualize-probe.frag.spv"},
                                                                                                          PipelineState{
                                                                                                              .cull_mode = CULL_MODE_BACK,
                                                                                                              .depth_op = COMPARE_OP_LESS_OR_EQUAL,
                                                                                                              .draw_mode = DRAWMODE_INDEXED_INDIRECT,
                                                                                                              .alpha_mode = ALPHA_MODE_OPAQUE,
                                                                                                              .depth_test = true,
                                                                                                              .depth_write = true,
                                                                                                          },
                                                                                                          attachment_info);

                    MaterialKey mat_key;
                    mat_key.custom_material = true;
                    mat_key.custom_material_id = probe_material->get_custom_material_id();

                    uint64_t pso_key = create_pso_key(PASS_MODE_FORWARD, mat_key.hash, MESH_TYPE_STATIC);
                    ShaderRegistry::get()->add(pso_key, probe_material->get_custom_shader());
                    scene->materials.push_back(std::move(probe_material));
                }

                uint32_t probe_count = irradiance_settings.get_probe_count();
                for (uint32_t i = 0; i < probe_count; ++i) {
                    Entity entity = scene->create_sphere("Probe" + std::to_string(i), parent);
                    irradiance_settings.debug_probes.push_back(entity);
                    TransformComponent *component = component_manager->get_component<TransformComponent>(entity);
                    component->position = irradiance_settings.probe_index_to_position(i);
                    component->scale = radius;

                    MeshComponent *mesh_comp = component_manager->get_component<MeshComponent>(entity);
                    mesh_comp->flags = MeshComponent::Flags::MESH_FLAG_DEBUG;
                    mesh_comp->primitives[0].material = material_index;
                }

                board->add<IrradianceFieldSettings>(irradiance_settings);
            },

            [](const DDGIGenerateRadiancePassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const IrradianceFieldSettings &settings = board->get<IrradianceFieldSettings>();

                // Update the debug probe visibility
                {
                    Scene *scene = renderer->get_scene();
                    auto &comp_manager = scene->ecs->component_manager;
                    for (auto &entity : settings.debug_probes) {
                        comp_manager->get_component<MeshComponent>(entity)->visible = settings.show_debug_probes;
                    }
                }

                if (!renderer->tlas.as.is_valid())
                    return;

                ScopedGpuProfiling(command_buffer, "DDGIRadiancePass");
                command_buffer->begin_gpu_debug_label("DDGIRadiancePass");

                struct PushData {
                    glm::mat4 random_orientation;

                    glm::ivec3 probe_counts;
                    uint32_t ray_per_probe;

                    glm::vec3 probe_start_position;
                    uint32_t frame_count;

                    glm::vec3 probe_step;
                    float _padding1;
                } push_data;

                push_data.random_orientation = GetDDGIRayRotation(renderer->frame_id);
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

        frame_graph->add_callback_pass<DDGIGenerateIrradiancePassData>(
            "DDGI Generate Irradiance",
            [&board](FrameGraph::Builder &builder, DDGIGenerateIrradiancePassData &data) {
                const IrradianceFieldSettings &irradiance_settings = board->get<IrradianceFieldSettings>();

                const glm::uvec3 &probe_counts = irradiance_settings.probe_counts;

                uint32_t width = IRRADIANCE_TEXTURE_DIM * probe_counts.x * probe_counts.y;
                uint32_t height = probe_counts.z * IRRADIANCE_TEXTURE_DIM;
                data.irradiance_texture = builder.create_texture("DDGIIrradianceTexture", {
                                                                                              .create_flags = 0,
                                                                                              // 8x8 interior + border
                                                                                              .width = width,
                                                                                              .height = height,
                                                                                              .depth = 1,
                                                                                              .mip_levels = 1,
                                                                                              .array_layers = 1,
                                                                                              .texture_type = TEXTURE_TYPE_2D,
                                                                                              .format = FORMAT_R16G16B16A16_SFLOAT,
                                                                                              .usage_flags = TEXTURE_USAGE_SAMPLED_BIT | TEXTURE_USAGE_STORAGE_BIT,
                                                                                          });
                builder.write(data.irradiance_texture, {
                                                           .access_flags = ACCESS_FLAG_SHADER_WRITE,
                                                           .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                           .layout = IMAGE_LAYOUT_GENERAL,
                                                       });

                const DDGIGenerateRadiancePassData &radiance_data = board->get<DDGIGenerateRadiancePassData>();
                builder.read(radiance_data.radiance_texture, {
                                                                 .access_flags = ACCESS_FLAG_SHADER_READ,
                                                                 .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                                 .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                             });
                builder.read(radiance_data.depth_texture, {
                                                              .access_flags = ACCESS_FLAG_SHADER_READ,
                                                              .stage_mask = PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                                              .layout = IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                          });

                data.shader = std::make_shared<ComputeShader>("DDGIGenerateIrradianceShader", "SPIRV/ddgi-irradiance.comp.spv");
                board->add<DDGIGenerateIrradiancePassData>(data);
            },

            [](const DDGIGenerateIrradiancePassData &data, const FrameGraphPassResource &pass_resource, void *context) {
                RenderContext *ctx = static_cast<RenderContext *>(context);
                Renderer *renderer = ctx->renderer;
                CommandBuffer *command_buffer = ctx->command_buffer;

                if (!renderer->tlas.as.is_valid())
                    return;

                FrameGraphBlackBoard *board = renderer->get_frame_graph_blackboard();
                const IrradianceFieldSettings &settings = board->get<IrradianceFieldSettings>();
                const glm::uvec3 &probe_counts = settings.probe_counts;

                ScopedGpuProfiling(command_buffer, "DDGIGenerateIrradiancePass");
                command_buffer->begin_gpu_debug_label("DDGGenerateIrradiancePass");

                struct PushData {
                    glm::mat4 random_orientation;
                    glm::ivec3 probe_counts;
                    int ray_per_probe;

                    uint32_t ddgi_radiance_texture_index;
                    uint32_t ddgi_depth_texture_index;
                    uint32_t _padding[2];
                } push_data;

                const DDGIGenerateRadiancePassData &radiance_pass_data = board->get<DDGIGenerateRadiancePassData>();

                push_data.random_orientation = GetDDGIRayRotation(renderer->frame_id);
                push_data.probe_counts = glm::ivec3(settings.probe_counts);
                push_data.ray_per_probe = settings.rays_per_probe;
                push_data.ddgi_radiance_texture_index = pass_resource.get<FrameGraphTexture>(radiance_pass_data.radiance_texture).id.id;
                push_data.ddgi_depth_texture_index = pass_resource.get<FrameGraphTexture>(radiance_pass_data.depth_texture).id.id;

                DescriptorOffset descriptors[] = {
                    renderer->get_or_create_descriptor(pass_resource.get<FrameGraphTexture>(data.irradiance_texture).id, DescriptorType::StorageImage),
                };

                data.shader->bind(command_buffer);

                uint32_t push_data_size = cast_u32(sizeof(push_data));
                command_buffer->set_push_data(0, &push_data, push_data_size);
                command_buffer->set_push_data(push_data_size, descriptors, cast_u32(sizeof(descriptors)));

                command_buffer->dispatch(probe_counts.x, probe_counts.y, probe_counts.z);

                command_buffer->end_gpu_debug_label();

                return;
            });
    }
} // namespace mirai