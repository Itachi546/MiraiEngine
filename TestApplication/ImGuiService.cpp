#include "ImGuiService.hpp"
#include "Device/Window.hpp"
#include "Common/HashMap.hpp"

#ifdef MIRAI_BACKEND_VULKAN
#include "Graphics/Vulkan/VulkanRenderingDevice.hpp"
#include "Graphics/Vulkan/VulkanSwapchain.hpp"
#include "Graphics/Vulkan/CommandBuffer.hpp"
#endif

#define IMGUI_IMPL_VULKAN_HAS_DYNAMIC_RENDERING
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

using namespace mirai;
namespace ImGuiService {
    static void check_vk_result(VkResult err) {
        if (err == 0)
            return;
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
            abort();
    }

    ImGuiID dockspace_id = 0;

    void Initialize() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
        // io.FontGlobalScale = 1.2f;
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

        VulkanRenderingDevice *device = (VulkanRenderingDevice *)RenderingDevice::get();
        ImGui::StyleColorsDark();

        GLFWwindow *window = (GLFWwindow *)Window::get()->get_window_ptr();
        ImGui_ImplGlfw_InitForVulkan(window, true);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = device->instance;
        init_info.PhysicalDevice = device->physical_device;
        init_info.Device = device->device;
        init_info.QueueFamily = device->queue_family_indices[QUEUE_TYPE_GRAPHICS];
        init_info.Queue = device->device_queues[QUEUE_TYPE_GRAPHICS];
        init_info.DescriptorPool = VK_NULL_HANDLE;
        init_info.DescriptorPoolSize = 1024;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(device->swapchain->images.size());
        init_info.UseDynamicRendering = true;

        VkFormat color_attachment_format = device->swapchain->surface_format.format;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        init_info.PipelineInfoMain.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_attachment_format;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);
        // ImGui_ImplVulkan_CreateFontsTexture();
    }

    HashMap<uint32_t, VkDescriptorSet> ImTextureIDMap;

    ImTextureID GetTextureID(uint32_t texture) {
        VulkanRenderingDevice *device = (VulkanRenderingDevice *)RenderingDevice::get();
        auto found = ImTextureIDMap.find(texture);
        ImTextureID textureId = K_INVALID_ID;
        if (found == ImTextureIDMap.end()) {
            VulkanTexture *vkTexture = device->access_texture(TextureID{texture});
            if (vkTexture->image_type != VK_IMAGE_TYPE_2D)
                return false;
            VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(VK_NULL_HANDLE, vkTexture->image_views[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            ImTextureIDMap[texture] = descriptorSet;
            textureId = (ImTextureID)descriptorSet;
        } else
            textureId = (ImTextureID)found->second;
        return textureId;
    }

    bool ImGuiService::AddImageButton(const char *id, uint32_t texture, const ImVec2 &size) {
        ImTextureID textureId = GetTextureID(texture);
        return ImGui::ImageButton(id, textureId, size);
    }

    void ImGuiService::AddImage(uint32_t texture, const ImVec2 &size, const ImVec4 &tint_color) {
        ImTextureID textureId = GetTextureID(texture);
        ImGui::Image(textureId, size, ImVec2{0.0, 0.0}, ImVec2{1.0, 1.0}, tint_color, ImVec4{0.0f, 0.0f, 0.0f, 0.0f});
    }

    void NewFrame() {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        // ImGui::DockSpaceOverViewport();
    }

    void Render(CommandBuffer *command_buffer) {
        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer->get_command_buffer());

        ImGuiIO &io = ImGui::GetIO();
        /*
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            // ImGui::UpdatePlatformWindows();
            // ImGui::RenderPlatformWindowsDefault();
        }
        */
    }

    void Shutdown() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        VulkanRenderingDevice *device = (VulkanRenderingDevice *)RenderingDevice::get();
    }
} // namespace ImGuiService