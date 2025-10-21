#include "ImGuiService.hpp"
#include "Device/Window.hpp"
#ifdef MIRAI_BACKEND_VULKAN
#include "Graphics/Vulkan/VulkanRenderingDevice.hpp"
#include "Graphics/Vulkan/Swapchain.hpp"
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
    VkDescriptorPool descriptor_pool = VK_NULL_HANDLE;

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

        descriptor_pool = device->create_descriptor_pool(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = device->instance;
        init_info.PhysicalDevice = device->physical_device;
        init_info.Device = device->device;
        init_info.QueueFamily = device->queue_family_indices[QUEUE_TYPE_GRAPHICS];
        init_info.Queue = device->device_queues[QUEUE_TYPE_GRAPHICS];
        init_info.DescriptorPool = descriptor_pool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(device->swapchain->images.size());
        init_info.UseDynamicRendering = true;

        VkFormat color_attachment_format = device->swapchain->surface_format.format;
        init_info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
        init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &color_attachment_format;
        init_info.CheckVkResultFn = check_vk_result;
        ImGui_ImplVulkan_Init(&init_info);
        // ImGui_ImplVulkan_CreateFontsTexture();
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
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            // ImGui::UpdatePlatformWindows();
            // ImGui::RenderPlatformWindowsDefault();
        }
    }

    void Shutdown() {
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        VulkanRenderingDevice *device = (VulkanRenderingDevice *)RenderingDevice::get();
        vkDestroyDescriptorPool(device->device, descriptor_pool, nullptr);
    }

} // namespace ImGuiService