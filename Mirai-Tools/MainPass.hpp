#include "Scene/FrameGraph.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "ImGuiService.hpp"

using namespace mirai;

struct MainPass : public FrameGraphRenderer {
  public:
    MainPass() : FrameGraphRenderer("main_pass") {
    }

    void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Scene *scene) override {
        command_buffer->begin_render_pass(node, frame_graph);
        ImGuiService::Render(command_buffer);
        command_buffer->end_render_pass();
    }

  private:
};