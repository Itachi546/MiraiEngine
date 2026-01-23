#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"
#include "Scene/RenderBatch.hpp"
#include "Math/Math.hpp"

namespace mirai {
    class CommandBuffer;
    class ShaderHashMap;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBuilder;

    struct GpuBufferSubAllocation {
        BufferID buffer;
        uint32_t offset;
        uint32_t size;

        void init(BufferID buffer, uint32_t size, uint32_t offset = 0) {
            this->buffer = buffer;
            this->size = size;
            this->offset = offset;
        }

        bool can_allocate(uint32_t required_size) {
            if (required_size >= (size - offset))
                return false;
            return true;
        }

        std::optional<BufferView> allocate(uint32_t required_size) {
            if (!can_allocate(required_size))
                return {};
            BufferView buffer_view;
            buffer_view.buffer = buffer;
            buffer_view.offset = offset;
            buffer_view.size = required_size;
            offset += required_size;
            return buffer_view;
        }
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

        // Used to preload shaders
        void set_pipeline_description_file(const std::string &pipeline_description_file) {
            this->pipeline_description_file = pipeline_description_file;
        }

        // Offset aligment align the offset address to the multiple of given value
        // E.g. minUniformBufferOffesetAlignment for uniform buffer
        uint32_t allocate_staging_buffer(uint32_t size, uint32_t current_frame, uint32_t offset_alignment = 64);

        ~Renderer();

        // Uniform Buffer
        BufferView cascade_uniform_buffer;
        BufferView per_frame_uniform_buffer;

        BufferID global_transform_buffer;
        BufferID global_material_buffer;

        BufferID per_frame_staging_buffer;
        uint8_t *per_frame_staging_buffer_ptr;

        // Global Geometry Buffer
        const uint32_t DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE = 64 * 1024 * 1024;
        GpuBufferSubAllocation vertex_buffer_allocator, index_buffer_allocator;

        // Per frame Uniform Set
        UniformSetID per_frame_uniform_set, vt_per_frame_uniform_set;
        UniformSetID transform_set, transform_material_set;
        std::vector<RenderBatch> main_render_batches;

        int jitter_index = 0;

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBuilder> frame_graph_builder;
        std::unique_ptr<ShaderHashMap> shader_hashmap;
        std::string pipeline_description_file;

        void copy_buffers();
        void update_uniform_set(CommandBuffer *cb);

        void initialize();

        void on_load_resources();

        void set_scene(std::unique_ptr<Scene> scene) {
            this->scene = std::move(scene);
        }

        void compile_passes();

        void update();

        void render();

        void patch_global_data(CommandBuffer *command_buffer);

        const uint32_t k_staging_buffer_size_per_frame = 4 * 1024 * 1024;
        const uint32_t k_transform_buffer_size = K_MAX_ENTITIES * 64;
        const uint32_t k_material_buffer_size = K_MAX_ENTITIES * sizeof(StandardPBRMaterial::PBRProperties);
        uint32_t per_frame_staging_buffer_offset = 0;

        const int JITTER_PERIOD = 8;

        friend class Engine;
    };
} // namespace mirai