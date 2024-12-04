#pragma once

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>

namespace mirai {
    class CommandBuffer;
}

namespace ImGuiService {

    void Initialize();

    void NewFrame();

    void Render(mirai::CommandBuffer *command_buffer);

    void Shutdown();

} // namespace ImGuiService