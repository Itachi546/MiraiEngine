#pragma once

#include "Vulkan.hpp"
#include <vector>

namespace mirai
{
    struct VulkanSwapchain
    {
        VkSwapchainKHR swapchain;
        std::vector<VkImage> images;
        std::vector<VkImageView> image_views;
        uint32_t width, height;
        uint32_t image_count;
        VkSurfaceFormatKHR surface_format;
        VkPresentModeKHR present_mode;
        VkCompositeAlphaFlagBitsKHR composite_mode;
        VkSurfaceTransformFlagBitsKHR current_transform;
    };

    bool PhysicalDeviceSupportPresentation(VkInstance instance, VkPhysicalDevice physicalDevice, uint32_t graphics_queue_index);

    VkSurfaceKHR CreateSurface(VkInstance instance, VkPhysicalDevice physical_device, uint32_t graphics_queue_index);

    void CreateSwapchain(VulkanSwapchain *swapchain, VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, bool vsync);

} // namespace mirai