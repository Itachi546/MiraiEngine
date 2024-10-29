#pragma once

#include <memory>
#include <vector>

#include "RenderingDevice.hpp"
#include "Scene/Scene.hpp"

#include <glm/glm.hpp>

namespace mirai
{
    class CommandBuffer;
    class ShaderMaterialCache;
    class FrameGraph;
    class FrameGraphBuilder;

    class Renderer
    {
      public:
        Renderer(bool enable_validation);
        Renderer(const Renderer &) = delete;
        Renderer operator=(const Renderer &) = delete;

        static Renderer *get()
        {
            return Instance;
        }

        void set_scene(std::unique_ptr<Scene> scene)
        {
            this->scene = std::move(scene);
        }

        Scene *get_scene()
        {
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
        std::unique_ptr<FrameGraph> frame_graph;
        std::unique_ptr<FrameGraphBuilder> frame_graph_builder;

   };
} // namespace mirai