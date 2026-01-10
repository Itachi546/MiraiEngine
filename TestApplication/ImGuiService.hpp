#pragma once

#define IMGUI_IMPL_VULKAN_NO_PROTOTYPES
#include <imgui.h>
#include <stdint.h>

namespace mirai {
    class CommandBuffer;
}

namespace ImGuiService {

    void Initialize();

    void NewFrame();

    void Render(mirai::CommandBuffer *command_buffer);

    inline bool IsAcceptingEvent() {
        return ImGui::IsAnyItemActive() || ImGui::IsAnyItemHovered();
    }

    bool AddImageButton(const char *id, uint32_t texture, const ImVec2 &size);
    void AddImage(uint32_t texture, const ImVec2 &size);

    void Shutdown();

} // namespace ImGuiService