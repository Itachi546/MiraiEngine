#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"
#include "Scene/RenderBatch.hpp"
#include "Math/Math.hpp"
#include "GPUResource.hpp"

namespace mirai {
    class CommandBuffer;
    class ShaderRegistryMap;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBlackBoard;
    class Renderer;
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

        // Offset aligment align the offset address to the multiple of given value
        // E.g. minUniformBufferOffesetAlignment for uniform buffer
        uint32_t allocate_staging_buffer(uint32_t size, uint32_t current_frame, uint32_t offset_alignment = 64);

        ~Renderer();

        // Uniform Buffer
        BufferView cascade_uniform_buffer;
        BufferView per_frame_uniform_buffer;

        BufferID global_transform_buffer;
        BufferID global_material_buffer;

        GPUResourceDescriptorHeap resource_heap;
        GPUSamplerDescriptorHeap sampler_heap;

        DescriptorOffset per_frame_data_descriptor;
        DescriptorOffset transform_descriptor;
        DescriptorOffset material_descriptor;
        DescriptorOffset global_geometry_descriptor;

        BufferID per_frame_staging_buffer;
        uint8_t *per_frame_staging_buffer_ptr;

        // Global Geometry Buffer
        const uint32_t DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE = 64 * 1024 * 1024;
        GpuBufferSubAllocation vertex_buffer_allocator, index_buffer_allocator;

        // Per frame Uniform Set
        std::vector<RenderBatch> main_opaque_batches;
        std::vector<RenderBatch> main_transparent_batches;
        std::vector<RenderBatch> main_alpha_mask_batches;
        std::vector<RenderBatch> main_skinned_batches;

        // TAA Options
        glm::mat4 prev_frame_VP;
        glm::vec2 prev_frame_jitter;
        glm::vec2 current_frame_jitter;

        int jitter_index = 0;
        int jitter_period = 4;

        uint32_t total_visible_entities = 0;

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBlackBoard> frame_graph_blackboard;
        std::unique_ptr<ShaderRegistryMap> shader_registry_map;

        void copy_buffers();

        void initialize();

        void on_load_resources();

        void set_scene(std::unique_ptr<Scene> scene) {
            this->scene = std::move(scene);
        }

        void compile_passes();

        void update();

        void render();

        void patch_global_data(CommandBuffer *command_buffer);

        void create_batches();
        void upload_batch_data(std::vector<RenderBatch> &batches, uint32_t current_frame);

        const uint32_t k_staging_buffer_size_per_frame = 4 * 1024 * 1024;
        const uint32_t k_transform_buffer_size = K_MAX_ENTITIES * 64;
        const uint32_t k_material_buffer_size = K_MAX_ENTITIES * K_MAX_MATERIAL_INSTANCE_DATA_SIZE;
        uint32_t per_frame_staging_buffer_offset = 0;
        uint32_t bindless_texture_count = 0;

        friend class Engine;
    };
} // namespace mirai