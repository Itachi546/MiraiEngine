#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"

#include "Math/Math.hpp"

namespace mirai {
    struct TextRenderManager;
    struct LineRenderer;
    class CommandBuffer;
    class ShaderManager;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBuilder;
    struct Font;

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

        void initialize();

        static Renderer *get() {
            return Instance;
        }

        void set_scene(std::unique_ptr<Scene> scene) {
            this->scene = std::move(scene);
        }

        Scene *get_scene() {
            return scene.get();
        }

        FrameGraph *get_frame_graph() { return frame_graph.get(); }

        void compile_passes();

        void update();

        void render();

        ~Renderer();

        bool enable_rt_shadow = false;
        BufferID transform_buffer;
        BufferID material_buffer;
        // Uniform Buffer
        BufferID cascade_uniform_buffer;
        BufferID per_frame_uniform_buffer;

        // Global Geometry Buffer
        const uint32_t DEFAULT_GEOMETRY_BUFFER_ALLOCATION_SIZE = 64 * 1024 * 1024;
        GpuBufferSubAllocation vertex_buffer_allocator, index_buffer_allocator;

        // Per frame Uniform Set
        UniformSetID per_frame_uniform_set;
        UniformSetID cascade_uniform_set;

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBuilder> frame_graph_builder;
        std::unique_ptr<TextRenderManager> text_render_manager;
        std::unique_ptr<Font> default_font;
        std::unique_ptr<LineRenderer> line_renderer;
        std::unique_ptr<ShaderManager> shader_manager;

        void copy_buffers(CommandBuffer *cb);

        const uint32_t k_staging_buffer_size_per_frame = 4 * 1024 * 1024;
        const uint32_t k_transform_buffer_size = K_MAX_ENTITIES * 64;
        const uint32_t k_material_buffer_size = K_MAX_ENTITIES * sizeof(StandardPBRMaterial::PBRProperties);
        BufferID per_frame_staging_buffer;
        uint8_t *per_frame_staging_buffer_ptr;
        uint32_t per_frame_staging_buffer_offset = 0;

        uint32_t allocate_staging_buffer(uint32_t size, uint32_t current_frame);
    };
} // namespace mirai