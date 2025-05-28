#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderManager.hpp"
#include "Scene/ShaderMaterial.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "TextRenderManager.hpp"
#include "LineRenderer.hpp"
#include "Common/Font.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"

#include <cstring>

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer() {
        ASSERT(Instance == nullptr);
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        texture_cache = std::make_unique<TextureCache>();
        miProfiler::Initialize();

        // Preload shaders
        shader_manager = std::make_unique<ShaderManager>();
        shader_manager->load("overlay_skybox", {"SPIRV/fullscreen.vert.spv", "SPIRV/skybox.frag.spv"}, {.depth_test = true});
        shader_manager->load("depth_prepass", {"SPIRV/depth_prepass.vert.spv"}, {.depth_test = true, .depth_write = true});
        shader_manager->load("pbr_forward", {"SPIRV/forward_pass.vert.spv", "SPIRV/forward_pass.frag.spv"}, {.depth_test = true, .depth_write = false});
        shader_manager->load("pbr_transparent", {"SPIRV/forward_pass.vert.spv", "SPIRV/transparent.frag.spv"}, {.cull_mode = CULL_MODE_NONE, .depth_test = true, .depth_write = true, .blend = true});
        shader_manager->load("gbuffer_pass", {"SPIRV/deferred.vert.spv", "SPIRV/deferred.frag.spv"}, {.depth_test = true, .depth_write = true});
        shader_manager->load("pbr_deferred", {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred_lighting.frag.spv"}, {.cull_mode = CULL_MODE_NONE});
        shader_manager->load("pbr_deferred_rt", {"SPIRV/fullscreen.vert.spv", "SPIRV/deferred_lighting_rt.frag.spv"}, {.cull_mode = CULL_MODE_NONE});
        shader_manager->load("csm_shadow", {"SPIRV/cascaded_shadow.vert.spv"}, {.cull_mode = CULL_MODE_NONE, .depth_test = true, .depth_write = true, .depth_clamp = true});
        shader_manager->load("swapchain_copy_rgba", {"SPIRV/fullscreen.vert.spv", "SPIRV/fullscreen.frag.spv"}, {.cull_mode = CULL_MODE_BACK});
        shader_manager->load("text_render_2d", {"SPIRV/font.vert.spv", "SPIRV/font.frag.spv"}, {.blend = true});
        shader_manager->load("line_3d", {"SPIRV/line.vert.spv", "SPIRV/line.frag.spv"}, {.depth_test = true, .topology = TOPOLOGY_LINE_LIST});

        frame_graph_builder = std::make_unique<FrameGraphBuilder>();
        frame_graph = std::make_unique<FrameGraph>(frame_graph_builder.get());

        text_render_manager = std::make_unique<TextRenderManager>();
        default_font = LoadFont("Georgia");
        text_render_manager->get_renderer_by_font(default_font.get());

        line_renderer = std::make_unique<LineRenderer>();
    }

    void Renderer::on_initialize() {
        this->scene->on_initialize();

        // Create acceleration structure for scene
        auto &render_list = scene->render_object_list;

        std::vector<AccelerationStructureMeshInfo> mesh_infos(render_list.size());
        for (uint32_t i = 0; i < render_list.size(); ++i) {
            RenderableObjectData &object = render_list[i];

            mesh_infos[i].vertex_buffer = {
                .buffer = object.vertex_buffer,
                .offset = cast_u32(object.vertex_offset * sizeof(Vertex)),
                .count = object.vertex_count,
                .stride = sizeof(Vertex),
            };

            mesh_infos[i].index_buffer = {
                .buffer = object.index_buffer,
                .offset = cast_u32(object.index_offset * sizeof(uint32_t)),
                .count = object.index_count,
                .stride = sizeof(uint32_t),
            };

            // @TODO a very long function :D
            auto transform_component = scene->component_manager->get_component_array<TransformComponent>()->components[object.transform_index];
            // The default representation of glm is column major while the VkTransformKHR uses row major
            // glm::mat4 transform = glm::transpose(transform_component.world_transform);
            glm::mat4 transform = transform_component.world_transform;
            for (int y = 0; y < 3; ++y) {
                for (int x = 0; x < 4; ++x) {
                    mesh_infos[i].transform[y][x] = transform[x][y];
                }
            }
        }
        device->create_acceleration_structure(mesh_infos.data(), cast_u32(mesh_infos.size()));

        frame_graph->compile();

        if (device->supports_raytracing())
            enable_rt_shadow = true;
    }
    void Renderer::copy_buffers(CommandBuffer *cb) {
        // @TODO do it here for now

        // Copy per frame uniform data
        uint32_t offset = 0;
        uint8_t *staging_buffer_ptr = scene->per_frame_staging_buffer_ptr;

        std::memcpy(staging_buffer_ptr, &scene->per_frame_data, sizeof(scene->per_frame_data));
        cb->copy_buffer(scene->per_frame_uniform_buffer, scene->per_frame_staging_buffer, {
                                                                                              .src_offset = offset,
                                                                                              .dst_offset = 0,
                                                                                              .size = sizeof(scene->per_frame_data),
                                                                                          });
        offset += sizeof(scene->per_frame_data);

        // Copy cascade info
        DirectionalLightCascadeInfo &cascade_info = scene->directional_light_info.cascade_info;
        std::memcpy(staging_buffer_ptr + offset, &cascade_info, sizeof(cascade_info));
        cb->copy_buffer(scene->cascade_uniform_buffer, scene->per_frame_staging_buffer, {
                                                                                            .src_offset = offset,
                                                                                            .dst_offset = 0,
                                                                                            .size = sizeof(cascade_info),
                                                                                        });
    }

    void Renderer::compile_passes() {
    }

    void Renderer::update() {
        line_renderer->NewFrame();
        scene->update();
        frame_graph->update(scene.get());

        FrameGraphNode *shadow_pass = frame_graph->get_node("directional_shadow_pass");
        FrameGraphNode *rt_shadow_pass = frame_graph->get_node("rt_directional_shadow_pass");
        rt_shadow_pass->enabled = enable_rt_shadow;
        shadow_pass->enabled = !enable_rt_shadow;
    }

    void Renderer::render() {

        device->new_frame();

        CommandBuffer *cb = device->get_command_buffer();
        cb->begin();

        miProfiler::BeginFrame(cb);
        {
            ScopedGpuProfiling(cb, "Gpu Time");

            // Copy per frame data from staging buffer to gpu uniform buffer
            copy_buffers(cb);

            frame_graph->render(cb, scene.get());

            device->queue_command_buffer(cb);
        }
        miProfiler::EndFrame();

        device->present();
    }

    Renderer::~Renderer() {
        shader_manager.reset();
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai