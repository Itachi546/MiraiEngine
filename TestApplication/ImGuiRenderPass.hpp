#pragma once

#include "Scene/FrameGraph.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#include "Engine/Profiler.hpp"
#include "ImGuiService.hpp"

using namespace mirai;

struct ImGuiRenderPass : public FrameGraphRenderer {
  public:
    ImGuiRenderPass() : FrameGraphRenderer("imgui_pass") {
    }

    void render(CommandBuffer *command_buffer, FrameGraph *frame_graph, FrameGraphNode *node, Renderer* renderer) override {
        device->begin_debug_utils_label(command_buffer, "ImGui Pass", nullptr);
        ScopedGpuProfiling(command_buffer, "ImGui Pass");
        command_buffer->begin_render_pass(node, frame_graph);
        ImGuiService::Render(command_buffer);
        command_buffer->end_render_pass();
        device->end_debug_utils_label(command_buffer);
    }

  private:
};