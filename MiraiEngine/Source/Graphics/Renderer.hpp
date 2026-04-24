#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"
#include "Scene/RenderBatch.hpp"
#include "Math/Math.hpp"
#include "GPUResource.hpp"
#include "Math/Frustum.hpp"
namespace mirai {
    class CommandBuffer;
    class ShaderRegistryMap;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBlackBoard;
    class Renderer;
    class ShadowSystem;
    struct LineRenderer;
    struct RenderContext {
        Renderer *renderer;
        CommandBuffer *command_buffer;
    };

    class Renderer {
      public:
        Renderer();
        Renderer(const Renderer &) = delete;
        Renderer operator=(const Renderer &) = delete;

        static Renderer *get() {
            return Instance;
        }

        Scene *get_scene() {
            return scene.get();
        }

        FrameGraph *get_frame_graph() { return frame_graph.get(); }
        FrameGraphBlackBoard *get_frame_graph_blackboard() {
            return frame_graph_blackboard.get();
        }

        void add_bindless_texture(TextureID texture);

        GPUBufferLinearAllocator *get_per_frame_allocator() {
            uint32_t frame_index = device->get_current_frame();
            ASSERT(frame_index < AppSettings::K_MAX_FRAME_IN_FLIGHTS);
            return &per_frame_allocator[frame_index];
        }

        // Helper function to upload batch data to per frame staging buffer
        void upload_batch_data(std::vector<RenderBatch> &batches, uint32_t current_frame);

        // Used for resources with default descriptor parameter
        DescriptorOffset get_or_create_descriptor(ID resource_id, DescriptorType descriptor_type);

        // For descriptor with custom parameter, we hash the string name and store it
        DescriptorOffset get_or_create_descriptor(const std::string &name, const DescriptorInfo &descriptor_info);

        ~Renderer();

        GPUResourceDescriptorHeap resource_heap;
        GPUSamplerDescriptorHeap sampler_heap;

        DescriptorOffset per_frame_light_descriptor;
        DescriptorOffset per_frame_data_descriptor;
        DescriptorOffset transform_descriptor;
        DescriptorOffset material_descriptor;
        DescriptorOffset global_geometry_descriptor;
        DescriptorOffset cascade_data_descriptor;

        // Global Geometry Buffer
        const uint32_t DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE = 64 * 1024 * 1024;
        GPUBufferAllocation vertex_buffer_allocator;
        GPUBufferAllocation index_buffer_allocator;

        // Uniform Buffer
        BufferID global_transform_buffer;
        BufferID global_material_buffer;

        // Per frame Uniform Set
        std::vector<RenderBatch> main_render_batches;

        // TAA Options
        glm::mat4 prev_frame_VP;
        glm::vec2 prev_frame_jitter;
        glm::vec2 current_frame_jitter;

        bool freeze_frustum = false;
        bool show_aabbs = false;
        FrustumPlanes freezed_frustum_planes;
        glm::mat4 freezed_inv_VP;

        int jitter_index = 0;
        int jitter_period = 4;

        uint32_t total_visible_lights = 0;
        uint32_t total_visible_entities = 0;

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBlackBoard> frame_graph_blackboard;
        std::unique_ptr<ShaderRegistryMap> shader_registry_map;
        std::unique_ptr<ShadowSystem> shadow_system;
        std::unique_ptr<LineRenderer> line_renderer;

        void copy_buffers();

        void initialize();

        void on_load_resources();

        void set_scene(std::unique_ptr<Scene> scene) {
            this->scene = std::move(scene);
        }

        void update();

        void render();

        void patch_global_data(CommandBuffer *command_buffer);

        void create_batches();

        void upload_visible_lights();

        const uint32_t k_staging_buffer_size_per_frame = 4 * 1024 * 1024;
        uint32_t bindless_texture_count = 0;
        GPUBufferLinearAllocator per_frame_allocator[AppSettings::K_MAX_FRAME_IN_FLIGHTS];
        HashMap<uint64_t, DescriptorOffset> descriptor_map;

        friend class Engine;
    };
} // namespace mirai
