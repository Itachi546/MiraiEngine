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
    class ShaderMaterialCache;
    class TextureCache;
    class FrameGraph;
    class FrameGraphBuilder;
    struct Font;

    class Renderer {
      public:
        Renderer();
        Renderer(const Renderer &) = delete;
        Renderer operator=(const Renderer &) = delete;

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

      private:
        static Renderer *Instance;
        std::unique_ptr<Scene> scene;
        std::unique_ptr<RenderingDevice> device;
        std::unique_ptr<ShaderMaterialCache> material_cache;
        std::unique_ptr<TextureCache> texture_cache;
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBuilder> frame_graph_builder;

        std::unique_ptr<TextRenderManager> text_render_manager;
        std::unique_ptr<Font> default_font;
        std::unique_ptr<LineRenderer> line_renderer;

        void copy_buffers(CommandBuffer *cb);
    };
} // namespace mirai