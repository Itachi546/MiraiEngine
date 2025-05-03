#include "Renderer.hpp"
#include "RenderingDevice.hpp"
#include "Vulkan/VulkanRenderingDevice.hpp"
#include "Vulkan/CommandBuffer.hpp"
#include "Scene/ShaderMaterialCache.hpp"
#include "Scene/TextureCache.hpp"
#include "Scene/Scene.hpp"
#include "Scene/FrameGraph.hpp"
#include "Engine/Engine.hpp"
#include "TextRenderManager.hpp"
#include "LineRenderer.hpp"
#include "Common/Font.hpp"
#include "Engine/Profiler.hpp"
#include "Device/Window.hpp"

namespace mirai {
    Renderer *Renderer::Instance = nullptr;

    Renderer::Renderer() {
        Instance = this;
        device = std::make_unique<VulkanRenderingDevice>();
        scene = std::make_unique<Scene>("default");
        material_cache = std::make_unique<ShaderMaterialCache>();
        texture_cache = std::make_unique<TextureCache>();
        miProfiler::Initialize();

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
                .offset = object.vertex_offset * sizeof(Vertex),
                .count = object.vertex_count,
                .stride = sizeof(Vertex),
            };

            mesh_infos[i].index_buffer = {
                .buffer = object.index_buffer,
                .offset = object.index_offset * sizeof(uint32_t),
                .count = object.index_count,
                .stride = sizeof(uint32_t),
            };

            // @TODO a very long function :D
            auto transform_component = scene->component_manager->get_component_array<TransformComponent>()->components[object.transform_index];
            std::memcpy(&mesh_infos[i].transform[0], &transform_component.world_transform[0], sizeof(glm::mat4));
        }
        device->create_acceleration_structure(mesh_infos.data(), cast_u32(mesh_infos.size()));
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
        miProfiler::Destroy();
        scene.reset();
    }

} // namespace mirai